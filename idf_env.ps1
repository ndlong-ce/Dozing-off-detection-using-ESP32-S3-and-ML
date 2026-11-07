Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH = "C:\esp\v5.5.5\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\ASUS\.espressif\python_env\idf5.5_py3.13_env"
& "$env:IDF_PATH\export.ps1"
