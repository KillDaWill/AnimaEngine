name: Feature request
description: Suggest a new extraction target, output format, or quality-of-life improvement.
labels: [enhancement]
body:
  - type: textarea
    id: problem
    attributes:
      label: Problem
      description: What are you trying to do that the current toolchain does not let you do?
    validations:
      required: true
  - type: textarea
    id: proposal
    attributes:
      label: Proposed solution
      description: Sketch the change. A CLI flag, a new output folder, a new parser, or a GUI control are all fine.
    validations:
      required: true
  - type: textarea
    id: alternatives
    attributes:
      label: Alternatives considered
      description: Anything you considered and rejected, and why.
    validations:
      required: false
  - type: input
    id: scope
    attributes:
      label: Affected files
      description: If you already know which source files would change, list them here.
    validations:
      required: false
