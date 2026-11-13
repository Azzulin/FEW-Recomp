"""Identity of the target game for the Python tools (the runtime's copy is runtime/include/hwder/game_profile.h).

Fire Emblem Warriors, US: base game v0 + update 1.5.0 (v327680) + add-on content.
"""
PROGRAM_ID = 0x0100F15003E64000          # base game (program NCA title ID, also inside the update's program NCA)
UPDATE_ID = PROGRAM_ID | 0x800           # 0x0100F15003E64800
AOC_BASE_ID = PROGRAM_ID + 0x1000        # add-on content is AOC_BASE_ID + index (1..5 for this game)
UPDATE_VERSION = 327680                  # 1.5.0: the ExeFS the generated code is built from
NAME = "Fire Emblem Warriors"
