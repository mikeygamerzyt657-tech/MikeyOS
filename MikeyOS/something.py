
from PIL import Image
import sys

if len(sys.argv) < 3:
    print("Usage: py something.py input.png output.rgb")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

WIDTH = 1920
HEIGHT = 1080

image = Image.open(input_file).convert("RGB")
image = image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)

with open(output_file, "wb") as f:
    f.write(image.tobytes())

print("RGB image generated!")
print(f"Resolution: {WIDTH} x {HEIGHT}")
print(f"File size: {WIDTH * HEIGHT * 3:,} bytes")
print(f"Output: {output_file}")
