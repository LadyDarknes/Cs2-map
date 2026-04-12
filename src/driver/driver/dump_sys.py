import sys
import os

with open('driver.sys', 'rb') as f:
    data = f.read()

hex_str = ','.join(f'0x{b:02X}' for b in data)

C_ARRAY = f"""#pragma once
namespace embedded_driver {{
static const unsigned char data[] = {{ {hex_str} }};
static const unsigned int size = sizeof(data);
}}
"""

with open('../ext/embedded_driver.h', 'w') as f:
    f.write(C_ARRAY)
print("Done")
