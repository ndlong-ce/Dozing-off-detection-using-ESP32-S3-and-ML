import cv2
import numpy as np
import onnxruntime as ort
from pathlib import Path

# ============================================================
# CONFIG
# ============================================================
MODEL_PATH = Path(__file__).with_name("eye_cnn.onnx")
IMG_SIZE = 32

# Labels must match the training code:
# Close-Eyes -> 0
# Open-Eyes  -> 1
LABELS = {
    0: "CLOSED",
    1: "OPEN",
}

# ============================================================
# LOAD ONNX MODEL
# ============================================================
if not MODEL_PATH.exists():
    raise FileNotFoundError(
        f"Cannot find model: {MODEL_PATH}\n"
        "Put eye_cnn.onnx in the same folder as this script."
    )

session = ort.InferenceSession(
    str(MODEL_PATH),
    providers=["CPUExecutionProvider"],
)

input_name = session.get_inputs()[0].name
input_shape = session.get_inputs()[0].shape

print("ONNX model loaded")
print("Input name :", input_name)
print("Input shape:", input_shape)

# ============================================================
# OPENCV CASCADES
# ============================================================
base = Path(cv2.data.haarcascades)

face_cascade = cv2.CascadeClassifier(
    str(base / "haarcascade_frontalface_default.xml")
)

eye_cascade = cv2.CascadeClassifier(
    str(base / "haarcascade_eye_tree_eyeglasses.xml")
)

if face_cascade.empty() or eye_cascade.empty():
    raise RuntimeError("Could not load OpenCV Haar Cascade files.")

# ============================================================
# PREPROCESSING
# Must match EyeDataset in the training code:
# grayscale -> 32x32 -> [0,1] -> [-1,1]
# ============================================================
def preprocess_eye(eye_gray):
    eye = cv2.resize(
        eye_gray,
        (IMG_SIZE, IMG_SIZE),
        interpolation=cv2.INTER_LINEAR,
    )

    x = eye.astype(np.float32) / 255.0
    x = (x - 0.5) / 0.5

    # [H,W] -> [1,1,H,W]
    x = x[np.newaxis, np.newaxis, :, :]
    return x


def predict_eye(eye_gray):
    x = preprocess_eye(eye_gray)

    logits = session.run(
        None,
        {input_name: x},
    )[0][0]

    # Softmax only for displaying confidence.
    exp_logits = np.exp(logits - np.max(logits))
    probs = exp_logits / np.sum(exp_logits)

    pred = int(np.argmax(logits))
    confidence = float(probs[pred])

    return pred, confidence, probs


# ============================================================
# WEBCAM
# ============================================================
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    raise RuntimeError(
        "Cannot open webcam. Try changing VideoCapture(0) to "
        "VideoCapture(1) if you have multiple cameras."
    )

print("\nControls:")
print("  Q / ESC : quit")
print("  S       : save current frame")
print()

while True:
    ret, frame = cap.read()

    if not ret:
        print("Cannot read frame from webcam.")
        break

    # Mirror view for a more natural webcam experience.
    frame = cv2.flip(frame, 1)

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # --------------------------------------------------------
    # 1. Detect face
    # --------------------------------------------------------
    faces = face_cascade.detectMultiScale(
        gray,
        scaleFactor=1.1,
        minNeighbors=5,
        minSize=(100, 100),
    )

    # Use the largest detected face.
    if len(faces) > 0:
        face = max(faces, key=lambda r: r[2] * r[3])
        fx, fy, fw, fh = face

        cv2.rectangle(
            frame,
            (fx, fy),
            (fx + fw, fy + fh),
            (255, 255, 255),
            2,
        )

        face_gray = gray[fy:fy + fh, fx:fx + fw]

        # ----------------------------------------------------
        # 2. Detect eyes inside the face
        # ----------------------------------------------------
        eyes = eye_cascade.detectMultiScale(
            face_gray,
            scaleFactor=1.1,
            minNeighbors=5,
            minSize=(25, 15),
        )

        # Sort roughly left-to-right and keep at most 2.
        eyes = sorted(eyes, key=lambda r: r[0])[:2]

        for i, (ex, ey, ew, eh) in enumerate(eyes):
            # Convert face coordinates -> frame coordinates.
            x1 = fx + ex
            y1 = fy + ey
            x2 = x1 + ew
            y2 = y1 + eh

            eye_gray = face_gray[ey:ey + eh, ex:ex + ew]

            if eye_gray.size == 0:
                continue

            pred, conf, probs = predict_eye(eye_gray)

            label = LABELS[pred]

            # Draw eye rectangle.
            cv2.rectangle(
                frame,
                (x1, y1),
                (x2, y2),
                (0, 255, 0) if pred == 1 else (0, 0, 255),
                2,
            )

            text = f"{label} {conf * 100:.1f}%"

            cv2.putText(
                frame,
                text,
                (x1, max(20, y1 - 8)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.65,
                (0, 255, 0) if pred == 1 else (0, 0, 255),
                2,
                cv2.LINE_AA,
            )

            # Show the 32x32 input that is actually sent to CNN.
            preview = cv2.resize(
                cv2.resize(eye_gray, (IMG_SIZE, IMG_SIZE)),
                (128, 128),
                interpolation=cv2.INTER_NEAREST,
            )

            # Position previews near the bottom.
            px = 10 + i * 150
            py = frame.shape[0] - 145

            if py >= 0 and px + 128 < frame.shape[1]:
                preview_bgr = cv2.cvtColor(preview, cv2.COLOR_GRAY2BGR)
                frame[py:py + 128, px:px + 128] = preview_bgr
                cv2.rectangle(
                    frame,
                    (px, py),
                    (px + 128, py + 128),
                    (255, 255, 255),
                    1,
                )
                cv2.putText(
                    frame,
                    f"Eye {i + 1}",
                    (px, py - 5),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.45,
                    (255, 255, 255),
                    1,
                    cv2.LINE_AA,
                )

    else:
        cv2.putText(
            frame,
            "No face detected",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (0, 0, 255),
            2,
            cv2.LINE_AA,
        )

    # Status / instructions.
    cv2.putText(
        frame,
        "Q/ESC: quit",
        (20, frame.shape[0] - 15),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        1,
        cv2.LINE_AA,
    )

    cv2.imshow("ESP32 Eye CNN - PC Webcam Test", frame)

    key = cv2.waitKey(1) & 0xFF

    if key == ord("q") or key == 27:
        break

    if key == ord("s"):
        filename = "eye_test_capture.png"
        cv2.imwrite(filename, frame)
        print(f"Saved: {filename}")

cap.release()
cv2.destroyAllWindows()
