#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont

def get_char_bytes(char, char_w, char_h, font):
    # 1. Создаем битмап для ОДНОГО символа
    img = Image.new('1', (char_w, char_h), color=1) # 1 - белый фон
    draw = ImageDraw.Draw(img)
    
    # Центрируем символ
    bbox = draw.textbbox((0, 0), char, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = (char_w - tw) // 2
    y = (char_h - th) // 2 - bbox[1]
    
    draw.text((x, y), char, font=font, fill=0) # 0 - черный текст

    # 2. Конвертируем этот битмап в байты (по страницам 8 пикселей)
    char_data = []
    for y_page in range(0, char_h, 8):
        for x_col in range(char_w):
            byte = 0
            for bit in range(8):
                py = y_page + bit
                if py < char_h:
                    if img.getpixel((x_col, py)) == 0:
                        byte |= (1 << bit)
            char_data.append(f"0x{byte:02x}")
    return char_data

text = "我爱你"
W, H = 24, 24  # Размер одного символа
font_path = "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc" 
font = ImageFont.truetype(font_path, int(H * 0.9))

# --- ОСНОВНОЙ ЦИКЛ ПО СИМВОЛАМ ---
for char in text:
    bytes_list = get_char_bytes(char, W, H, font)
    
    print("{", end=" ")
    print(f"// {char}")
    print("  " + ", ".join(bytes_list))
    print("},\n")
