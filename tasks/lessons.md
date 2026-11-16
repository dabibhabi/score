# Lessons

## 2026-10-17 — Deep-dive deliverables are tutorials, not recaps

- **Mistake:** Treated "update the artifact with engineering questions and resources" as a light
  edit — bolting question boxes onto an existing case-study page.
- **Correction:** The user wants a full deep-dive *tutorial*: rigorous worked examples,
  step-by-step explanations, academic references and historical examples, engineering questions
  they answer themselves. Both the HTML artifact and the PDF must be dark mode; the PDF uses both
  `academic-pdf-deepdive` and `dark-diagram-pdf`; the artifact needs real motion design.
- **Rule:** For any "deep dive" request, default to tutorial depth + dark mode, build the HTML and
  PDF with parallel agents sharing one fact sheet, and save sources/outputs to `artifacts/`.
