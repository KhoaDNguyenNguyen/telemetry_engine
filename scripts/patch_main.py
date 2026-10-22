import os

main_c = 'app/main.c'
with open(main_c, 'r') as f:
    content = f.read()

replacements = {
    'int dino_w = 30, dino_h = 30;': 'int dino_w = 48, dino_h = 48;',
    'dino_h = (is_ducking && dino_y == 180.0f) ? 15 : 30;': 'dino_h = (is_ducking && dino_y == 180.0f) ? 24 : 48;',
    'obs_h = obs_type ? 15 : 30; obs_w = obs_type ? 20 : 15;': 'obs_h = obs_type ? 24 : 48; obs_w = obs_type ? 40 : 32;',
    'speed += 0.2f; if (speed > 14.0f) speed = 14.0f;': 'speed += 0.5f; if (speed > 22.0f) speed = 22.0f;',
    'obs_x < -20.0f': 'obs_x < -50.0f'
}

for k, v in replacements.items():
    content = content.replace(k, v)

with open(main_c, 'w') as f:
    f.write(content)
