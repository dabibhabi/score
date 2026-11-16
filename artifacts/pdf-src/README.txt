score deep dive (dark mode) -- rebuild instructions
====================================================
Build the PDF (XeLaTeX; TeX Live 2026 with Carlito, DejaVu Sans Mono, Latin Modern Math):
    cd pdf-src && xelatex main.tex && xelatex main.tex && xelatex main.tex

Layout:
  main.tex             preamble (dark palette, boxes, listing styles) + \input of chapters
  chapters/            frontmatter, ch1..ch9, appendix (hints + reproduction), bibliography
  diagrams/*.mmd       Mermaid sources; rendered with mermaid-cli 12:
                         mmdc -i X.mmd -o X.pdf -c mermaid-dark.json -b '#0D1117'
  diagrams/*.svg       hand-authored SVG; rendered with: rsvg-convert -f pdf -o X.pdf X.svg
  figures/gen_bits.py  regenerates figures/bits.tex (binary64 bit fields)
  data/*.dat           pgfplots data; regenerate with verify/make_plot_data.py (run inside verify/)
  repo/                verbatim snapshots of the repository files quoted by \lstinputlisting
                       (taken 2026-10-18, after the HistoricalTail comment fix in risk.cpp)
  verify/              every program whose output is quoted in the text (see Appendix B)

Verification programs compile with, e.g.:
    clang++ -std=c++23 -I<repo>/include -I<repo>/external/json/single_include \
        risk_check.cpp <repo>/src/statistics/risk.cpp -o risk_check
