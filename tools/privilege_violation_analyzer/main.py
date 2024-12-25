#!/usr/bin/env python3

import sys
from elftools.elf.elffile import ELFFile

from section_classifier import SectionClassifier
from symbol_analyzer import SymbolAnalyzer
from disassembly_analyzer import DisassemblyAnalyzer
from call_graph import CallGraphBuilder

def main(filename):
    with open(filename, 'rb') as f:
        elffile = ELFFile(f)

        # Step 1: Classify Sections
        classifier = SectionClassifier()
        section_dict = classifier.gather_sections(elffile)

        # Step 2: Analyze Symbols
        symbol_analyzer = SymbolAnalyzer(elffile, section_dict)
        symbol_analyzer.analyze_symbols()
        symbols = symbol_analyzer.get_text_symbols()

        # Step 3: Disassemble symbol instructions
        disassembler = DisassemblyAnalyzer(elffile, symbol_analyzer.get_addr_to_sym_map())
        for sym in symbols:
            sym["instructions"] = disassembler.disassemble_symbol(sym)
    

        # Step 4: Construct a call graph and detect privilege violations
        call_graph_builder = CallGraphBuilder(symbols)
        call_graph_builder.build_graph()

        # Print privilege violations
        call_graph_builder.print_privilege_violations()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python main.py <file.elf>")
        sys.exit(1)
    main(sys.argv[1])
