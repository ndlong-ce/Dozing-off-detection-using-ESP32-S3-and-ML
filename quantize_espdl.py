"""
eye_cnn.onnx -> eye_cnn.espdl (INT8, ESP32-S3)

Cai dat:
    pip install git+https://github.com/espressif/esp-ppq.git onnx onnxsim pillow torch
    (Windows loi thi chay tren WSL / Kaggle / Colab)

Chay:
    python quantize_espdl.py --data-root "D:/VisualStudioCode/Embedded_dozing_off/mrleyedataset"
"""
import argparse
import random
from pathlib import Path

import numpy as np
import torch
from PIL import Image
from torch.utils.data import DataLoader, Dataset
from esp_ppq.api import espdl_quantize_onnx

IMG_SIZE = 32
EXTS = {".png", ".jpg", ".jpeg", ".bmp"}


class CalibDataset(Dataset):
    """Anh hieu chuan: can bang open/closed, tien xu ly Y HET luc train: (x/255-0.5)/0.5."""

    def __init__(self, root, close_dir, open_dir, n_per_class):
        rng = random.Random(0)
        files = []
        for d in (close_dir, open_dir):
            fs = [p for p in (Path(root) / d).rglob("*") if p.suffix.lower() in EXTS]
            rng.shuffle(fs)
            files += fs[:n_per_class]
        self.files = files

    def __len__(self):
        return len(self.files)

    def __getitem__(self, i):
        img = Image.open(self.files[i]).convert("L").resize((IMG_SIZE, IMG_SIZE), Image.BILINEAR)
        x = (np.asarray(img, dtype=np.float32) / 255.0 - 0.5) / 0.5
        return torch.from_numpy(x).unsqueeze(0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", required=True)
    ap.add_argument("--close-dir", default="Close-Eyes")
    ap.add_argument("--open-dir", default="Open-Eyes")
    ap.add_argument("--onnx", default="eye_cnn.onnx")
    ap.add_argument("--out", default="eye_cnn.espdl")
    ap.add_argument("--calib-per-class", type=int, default=512)
    ap.add_argument("--batch", type=int, default=32)
    args = ap.parse_args()

    ds = CalibDataset(args.data_root, args.close_dir, args.open_dir, args.calib_per_class)
    dl = DataLoader(ds, batch_size=args.batch, shuffle=False)

    espdl_quantize_onnx(
        onnx_import_file=args.onnx,
        espdl_export_file=args.out,
        calib_dataloader=dl,
        calib_steps=len(dl),
        input_shape=[1, 1, IMG_SIZE, IMG_SIZE],
        target="esp32s3",
        num_of_bits=8,
        collate_fn=lambda x: x.to("cpu"),
        device="cpu",
        error_report=True,      # in sai so tung lop: lop nao > ~10% la dang ngo
        export_test_values=True,
        verbose=1,
    )
    print("done ->", args.out)


if __name__ == "__main__":
    main()
