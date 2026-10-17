
from PIL import Image

input_file = "d.png"
output_file = "a.png"

# Open the original image and preserve transparency support.
image = Image.open(input_file).convert("RGBA")
pixels = image.load()

width, height = image.size
removed = 0

for y in range(height):
    for x in range(width):
        r, g, b, a = pixels[x, y]

        # Make white and near-white pixels transparent.
        if r >= 245 and g >= 245 and b >= 245:
            pixels[x, y] = (255, 255, 255, 0)
            removed += 1

image.save(output_file, "PNG")

print("Finished removing white pixels!")
print(f"Image size: {width} x {height}")
print(f"Pixels made transparent: {removed:,}")
print(f"Saved as: {output_file}")
