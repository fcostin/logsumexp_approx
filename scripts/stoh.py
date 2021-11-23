"""

stoh -- aka .s to .h

purpose:

Assemble the given gnu assembler file, assumed to contain
code for a single function in the .text section, and write
the resulting bytes of machine code to the given output
header file that could be imported from a C program

python3 as.py --in-file demo.s --out-file foo
"""

import argparse
import collections
import subprocess
import tempfile
import os.path
import re


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--in-file', type=str, required=True, help='input assembly file')
    p.add_argument('--out-file', type=str, required=True, help='output header file')
    return p.parse_args()


BEGIN_SECTION_RE = re.compile(r"^Disassembly of section (.*):$")

GUARD_HEADER = """
#ifndef %s
#define %s 1


"""
GUARD_FOOTER = """

#endif
"""


def parse_objdump_disassembly(output):
    # parsing objdump human readable output
    # is probably going to be very fragile.

    lines = output.split('\n')
    current_section = None
    records_by_section = collections.defaultdict(list)

    section_names = [] # track order

    for line in lines:
        line = line.strip()
        m = BEGIN_SECTION_RE.match(line)
        if m is not None:
            section_name = m.group(1)
            current_section = section_name
            section_names.append(section_name)

        if current_section is None:
            continue

        records = records_by_section[current_section]

        tokens = line.split('\t')
        tokens = [x.strip() for x in tokens]
        tokens = [x for x in tokens if x]
        if len(tokens) == 3:
            machinecode = tokens[1].split()
            comment = tokens[2]
            records.append((machinecode, comment))
        elif len(tokens) == 2:
            machinecode = tokens[1].split()
            comment = ''
            records.append((machinecode, comment))

    return section_names, records_by_section


def format_defn_lines(name, records):
    """    
    const unsigned char CODE_LOAD_A0_XMM3[] = {
        0xc5, 0xfb, 0x10, 0x1f //vmovsd (%rdi),%xmm3
    };
    """
    lines = []
    emit = lines.append

    emit("const unsigned char %s[] = {" % (name, ))
    n_records = len(records)
    for i, r in enumerate(records):
        is_last = i == n_records - 1
        machinecode, description = r
        oxcode = ['0x%s' % (hh, ) for hh in machinecode]
        codestring = ', '.join(oxcode)
        if not is_last:
            codestring = codestring + ','
        if description:
            codestring += ' //' + description
        codestring = '\t' + codestring
        emit(codestring)
    emit("};")
    emit("")

    return lines


def as_c_constant_name(name):
    return name.upper()


def as_guard_name(filename):
    cs = [c.upper() if c.isalpha() else '_' for c in filename]
    return ''.join(cs)


def main():
    args = parse_args()

    # gcc -std=c99 -c in.s -o out.o
    with tempfile.TemporaryDirectory() as temp_dir:
        object_fn = os.path.join(temp_dir, 'a.o')
        gcc_args = ['gcc', '-std=c99', '-c', args.in_file, '-o', object_fn]
        subprocess.run(gcc_args, check=True)

        objdump_args = ['objdump', '-D', object_fn]
        results = subprocess.run(objdump_args, check=True, capture_output=True, text=True)
        section_names, records_by_section = parse_objdump_disassembly(results.stdout)

        all_defn_lines = []

        for section_name in section_names:
            records = records_by_section[section_name]
            name = as_c_constant_name(section_name)
            defn_lines = format_defn_lines(name, records)
            all_defn_lines.extend(defn_lines)

        with open(args.out_file, 'w') as f_out:
            guard_name = as_guard_name(args.out_file)
            f_out.write(GUARD_HEADER % (guard_name, guard_name))
            for line in all_defn_lines:
                data = line + '\n'
                f_out.write(data)
            f_out.write(GUARD_FOOTER)

        print('wrote %s'  % (args.out_file, ))


if __name__ == '__main__':
    main()
        
