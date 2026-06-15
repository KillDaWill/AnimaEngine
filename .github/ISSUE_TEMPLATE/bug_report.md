name: Bug report
description: Report a wrong output, a crash, or a build failure.
labels: [bug]
body:
  - type: dropdown
    id: game
    attributes:
      label: Which game ROM were you using?
      description: Pick the title that produced the issue.
      options:
        - Pokemon Black
        - Pokemon White
        - Pokemon Black 2
        - Pokemon White 2
        - Other / not applicable
    validations:
      required: true
  - type: input
    id: dex
    attributes:
      label: Pokémon dex number
      description: National dex number of the species you were extracting (for example 25 for Pikachu). Leave blank if not applicable.
  - type: input
    id: cmd
    attributes:
      label: Command you ran
      description: Paste the exact command line, with the path to the ROM redacted if you want.
    validations:
      required: true
  - type: textarea
    id: expected
    attributes:
      label: Expected behaviour
      description: What did you expect the tool to produce?
    validations:
      required: true
  - type: textarea
    id: actual
    attributes:
      label: Actual behaviour
      description: What did the tool actually produce? Paste the terminal output, an image link, or both.
    validations:
      required: true
  - type: textarea
    id: env
    attributes:
      label: Environment
      description: OS, CPU, raylib and libpng versions (`pkg-config --modversion raylib libpng`), and `AnimaEngine --version` if it printed one.
    validations:
      required: false
  - type: checkboxes
    id: ip
    attributes:
      label: Repository rules
      description: Please confirm before submitting.
      options:
        - label: I understand this issue tracker is not for ROM or asset requests.
          required: true
        - label: I am not attaching a ROM, save file, or extracted Pokémon asset.
          required: true
