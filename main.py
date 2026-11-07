"""
Eye state (open/closed) CNN for ESP32-S3 + ESP-DL.

Dataset layout expected (labels come from the folder name):
    <data-root>/Close-Eyes/*.png   -> label 0
    <data-root>/Open-Eyes/*.png    -> label 1

If the files still carry MRL names
(sXXXX_YYYYY_gender_glasses_state_reflection_lighting_sensor.png) the subject
ID and the glasses flag are recovered from the filename, which is what makes a
correct subject-wise split possible. If they don't, the script falls back to a
random split and says so loudly.

Output: ONNX file ready for esp-ppq quantization.

Usage (Windows):
    python train_eye_cnn.py --data-root "D:/VisualStudioCode/Embedded_dozing_off/mrleyedataset"
"""

import argparse
import random
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image
from torch.utils.data import DataLoader, Dataset

IMG_SIZE = 32
SEED = 42
EXTS = {".png", ".jpg", ".jpeg", ".bmp"}

# a leading sXXXX token is the MRL subject ID
SUBJECT_RE = re.compile(r"^(s\d+)", re.IGNORECASE)


# --------------------------------------------------------------------------
# Dataset indexing
# --------------------------------------------------------------------------

def parse_name(path: Path):
    """
    Recover what we can from an MRL filename. Label is NOT taken from here --
    it comes from the folder -- but subject and glasses are, and subject is the
    field the whole evaluation depends on.
    """
    parts = path.stem.split("_")
    subject = None
    m = SUBJECT_RE.match(parts[0]) if parts else None
    if m:
        subject = m.group(1).lower()

    glasses = -1
    if len(parts) >= 8:
        try:
            glasses = int(parts[3])
        except ValueError:
            pass

    return subject, glasses


def index_dataset(root: Path, close_name: str, open_name: str):
    """Walk both class folders. Returns (by_subject, had_subject_ids)."""
    by_subject = defaultdict(list)
    counts = {0: 0, 1: 0}
    missing_subject = 0

    for folder, label in ((close_name, 0), (open_name, 1)):
        d = root / folder
        if not d.is_dir():
            sys.exit(f"ERROR: folder not found: {d}")
        for p in d.rglob("*"):
            if p.suffix.lower() not in EXTS:
                continue
            subject, glasses = parse_name(p)
            if subject is None:
                subject = "_unknown"
                missing_subject += 1
            by_subject[subject].append((p, {"label": label, "glasses": glasses}))
            counts[label] += 1

    if not any(counts.values()):
        sys.exit(f"ERROR: no images found under {root}")

    print(f"closed: {counts[0]}   open: {counts[1]}")
    had_ids = missing_subject == 0 and "_unknown" not in by_subject
    if not had_ids:
        print(f"WARNING: {missing_subject} files have no sXXXX subject ID in the name.")
    else:
        print(f"subjects found: {len(by_subject)}")
    return by_subject, had_ids


def split_by_subject(by_subject, n_val, n_test):
    """
    CRITICAL: split by person, not by image. MRL has ~37 people and thousands of
    near-duplicate frames each; a random split leaks the same eye into train and
    test and inflates accuracy by several points.
    """
    subjects = sorted(by_subject.keys())
    if len(subjects) < n_val + n_test + 2:
        print("WARNING: too few subject IDs for a subject-wise split. "
              "Falling back to a RANDOM split -- test accuracy will be "
              "optimistic and is NOT a valid estimate of on-device accuracy.")
        flat = [s for subj in subjects for s in by_subject[subj]]
        rng = random.Random(SEED)
        rng.shuffle(flat)
        n = len(flat)
        n_te, n_va = int(0.15 * n), int(0.15 * n)
        return flat[n_te + n_va:], flat[n_te:n_te + n_va], flat[:n_te], None

    rng = random.Random(SEED)
    rng.shuffle(subjects)
    test_s = subjects[:n_test]
    val_s = subjects[n_test:n_test + n_val]
    train_s = subjects[n_test + n_val:]

    collect = lambda names: [s for n in names for s in by_subject[n]]
    return collect(train_s), collect(val_s), collect(test_s), (train_s, val_s, test_s)


class EyeDataset(Dataset):
    """
    Augmentation deliberately mimics the noise of a real ESP-DL landmark crop:
    the eye centre jitters, the crop scale drifts with head distance, and the
    head tilts. Training without this makes the model brittle on device.
    """

    def __init__(self, samples, train: bool):
        self.samples = samples
        self.train = train

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        path, meta = self.samples[idx]
        img = Image.open(path).convert("L")

        if self.train:
            s = random.uniform(0.85, 1.15)           # driver distance varies
            side = max(8, int(IMG_SIZE * s))
            img = img.resize((side, side), Image.BILINEAR)
            img = img.rotate(random.uniform(-12, 12), resample=Image.BILINEAR)
            dx, dy = random.randint(-3, 3), random.randint(-3, 3)   # crop jitter
            img = img.transform(img.size, Image.AFFINE, (1, 0, dx, 0, 1, dy),
                                resample=Image.BILINEAR)
            img = img.resize((IMG_SIZE, IMG_SIZE), Image.BILINEAR)
            if random.random() < 0.5:                # one model for both eyes
                img = img.transpose(Image.FLIP_LEFT_RIGHT)
        else:
            img = img.resize((IMG_SIZE, IMG_SIZE), Image.BILINEAR)

        x = np.asarray(img, dtype=np.float32) / 255.0

        if self.train:                               # IR illuminator varies
            x = np.clip(x * random.uniform(0.75, 1.25)
                        + random.uniform(-0.12, 0.12), 0, 1)

        x = (x - 0.5) / 0.5                          # -> [-1, 1]
        return torch.from_numpy(x).unsqueeze(0), meta["label"], meta["glasses"]


# --------------------------------------------------------------------------
# Model
# --------------------------------------------------------------------------

class EyeNet(nn.Module):
    """
    ~7k params, ~0.66 MMAC. Only Conv / BN / ReLU / MaxPool / GAP / Linear,
    because esp-ppq quantizes these cleanly. No depthwise, no SE, no Swish.
    """

    def __init__(self, n_classes=2):
        super().__init__()
        self.block1 = self._block(1, 8)
        self.block2 = self._block(8, 16)
        self.block3 = self._block(16, 32)
        self.head = nn.Linear(32, n_classes)

    @staticmethod
    def _block(cin, cout):
        return nn.Sequential(
            nn.Conv2d(cin, cout, 3, padding=1, bias=False),
            nn.BatchNorm2d(cout),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),
        )

    def forward(self, x):
        x = self.block3(self.block2(self.block1(x)))
        x = F.adaptive_avg_pool2d(x, 1).flatten(1)
        return self.head(x)


# --------------------------------------------------------------------------
# Train / eval
# --------------------------------------------------------------------------

@torch.no_grad()
def evaluate(model, loader, device):
    model.eval()
    correct = total = 0
    g_correct = g_total = 0          # glasses subgroup: dominant failure mode
    for x, y, glasses in loader:
        x, y = x.to(device), y.to(device)
        hit = (model(x).argmax(1) == y)
        correct += hit.sum().item()
        total += y.numel()
        mask = glasses.to(device) == 1
        g_correct += hit[mask].sum().item()
        g_total += mask.sum().item()
    return correct / total, (g_correct / g_total if g_total else float("nan"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", type=str,
                    default=r"D:/VisualStudioCode/Embedded_dozing_off/mrleyedataset")
    ap.add_argument("--close-dir", type=str, default="Close-Eyes")
    ap.add_argument("--open-dir", type=str, default="Open-Eyes")
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--batch-size", type=int, default=256)
    ap.add_argument("--lr", type=float, default=3e-3)
    ap.add_argument("--n-val", type=int, default=5)
    ap.add_argument("--n-test", type=int, default=5)
    ap.add_argument("--workers", type=int, default=0,
                    help="0 is safest on Windows; raise to 4 on Linux/Kaggle")
    ap.add_argument("--out", type=str, default="eye_cnn.onnx")
    args = ap.parse_args()

    torch.manual_seed(SEED)
    np.random.seed(SEED)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"device: {device}")

    by_subject, _ = index_dataset(Path(args.data_root), args.close_dir, args.open_dir)
    train_s, val_s, test_s, names = split_by_subject(by_subject, args.n_val, args.n_test)
    if names:
        print(f"subjects  train={len(names[0])} val={len(names[1])} test={len(names[2])}")
        print(f"test subjects held out: {names[2]}")
    print(f"images    train={len(train_s)} val={len(val_s)} test={len(test_s)}")

    def mk(s, tr):
        return DataLoader(EyeDataset(s, tr), batch_size=args.batch_size, shuffle=tr,
                          num_workers=args.workers,
                          pin_memory=(device == "cuda"), drop_last=tr)

    train_dl, val_dl, test_dl = mk(train_s, True), mk(val_s, False), mk(test_s, False)

    model = EyeNet().to(device)
    print(f"params: {sum(p.numel() for p in model.parameters())}")

    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.OneCycleLR(
        opt, max_lr=args.lr, total_steps=args.epochs * len(train_dl))
    # label smoothing keeps logits out of the saturation range where int8
    # quantization error would flip a prediction
    crit = nn.CrossEntropyLoss(label_smoothing=0.05)

    best = 0.0
    for ep in range(1, args.epochs + 1):
        model.train()
        running = 0.0
        for x, y, _ in train_dl:
            x, y = x.to(device), y.to(device)
            opt.zero_grad()
            loss = crit(model(x), y)
            loss.backward()
            opt.step()
            sched.step()
            running += loss.item()
        acc, g_acc = evaluate(model, val_dl, device)
        print(f"ep {ep:3d}  loss {running/len(train_dl):.4f}  "
              f"val {acc:.4f}  val(glasses) {g_acc:.4f}")
        if acc > best:
            best = acc
            torch.save(model.state_dict(), "eye_cnn_best.pt")

    model.load_state_dict(torch.load("eye_cnn_best.pt"))
    acc, g_acc = evaluate(model, test_dl, device)
    print(f"\nTEST  acc {acc:.4f}   glasses-only {g_acc:.4f}")

    # ---- export for esp-ppq -------------------------------------------------
    model.eval().cpu()
    fused = torch.ao.quantization.fuse_modules(
        model, [[f"block{i}.0", f"block{i}.1", f"block{i}.2"] for i in (1, 2, 3)])
    torch.onnx.export(fused, torch.randn(1, 1, IMG_SIZE, IMG_SIZE), args.out,
                      input_names=["input"], output_names=["logits"],
                      opset_version=13, do_constant_folding=True)
    print(f"exported {args.out}")


if __name__ == "__main__":
    main()