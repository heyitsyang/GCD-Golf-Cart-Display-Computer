#
# This program patches a bug in EEZ Studio's Flow
# code generation where an extra prameter is added
#

import os
import re

file_path = os.path.join('src', 'ui', 'eez-flow.cpp')

# Regex to match:
# lv_dropdown_set_selected(anything1, anything2, anything3);
pattern = re.compile(
    r'(lv_dropdown_set_selected\s*\(\s*'       # function name and opening (
    r'([^,]+)\s*,\s*'                          # first argument (group 2)
    r'([^,]+)\s*,\s*'                          # second argument (group 3)
    r'[^)]+?\s*\)\s*;)'                        # third argument (will be omitted)
)

replacement_count = 0
new_lines = []

with open(file_path, 'r', encoding='utf-8') as file:
    lines = file.readlines()

for line in lines:
    match = pattern.search(line)
    if match:
        indent = line[:len(line)-len(line.lstrip())]
        first_arg = match.group(2).strip()
        second_arg = match.group(3).strip()
        # Build the replacement line, preserving indentation and newline
        new_line = f"{indent}lv_dropdown_set_selected({first_arg}, {second_arg});\n"
        new_lines.append(new_line)
        replacement_count += 1
    else:
        new_lines.append(line)

with open(file_path, 'w', encoding='utf-8') as file:
    file.writelines(new_lines)

print(f'Replacement complete. {replacement_count} occurrence(s) modified.')

