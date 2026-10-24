"""Scenarios of the equivalence test (tests/det/det.py): scripts of input keyed to GAME frames, run on the
deterministic build (make DET=1: fixed 24 tic steps, constant random seed, binary pad).

An item is ('until', N)   wait until game frame N (and print the state hash there),
            ('tap', X, K)  press X for one frame and release it for one, K times (from the current frame),
            ('hold', X)   pad: up/down/left/right/1/2 joined with '+', or none,
            ('poke', SYM, VAL)  write a byte of system RAM,
            ('warp', x, y, dir) teleport (dir 0 N, 1 E, 2 S, 3 W),
            ('map', n)    jump to level n (1..10).
Every game frame's screen is hashed too (smstest hooklog at the entry of ThreeDRefresh), so a build that
renders or plays differently in ANY frame of ANY scenario is caught."""

SCENARIOS = {
    # standing at the start: the patrolling guards of the first rooms move about
    'idle': [('poke', 'dbg_god', 1), ('until', 50)],
    # the start door, the corridor, a second door; turns
    'walk': [('poke', 'dbg_god', 1), ('until', 3), ('hold', 'up'), ('until', 8), ('hold', '2'), ('until', 11),
             ('hold', 'up'), ('until', 22), ('hold', '2'), ('until', 25), ('hold', 'up'), ('until', 40),
             ('hold', 'left'), ('until', 46), ('hold', 'up'), ('until', 60), ('hold', 'none'), ('until', 64)],
    # every ray changes every frame
    'turn': [('poke', 'dbg_god', 1), ('until', 3), ('hold', 'left'), ('until', 30), ('hold', 'right'), ('until', 50),
             ('hold', 'none'), ('until', 52)],
    # shooting a standing guard: AI, damage, death, drops, score
    'fight': [('poke', 'dbg_god', 1), ('until', 2), ('warp', 33, 11, 0), ('until', 4), ('tap', '1', 24), ('until', 56),
              ('hold', 'up'), ('until', 64), ('hold', 'none'), ('until', 66)],
    # no god mode, standing still in front of the guards: damage, the face, the red flash, death, respawn
    'hurt': [('until', 2), ('warp', 33, 12, 0), ('until', 40), ('hold', 'right'), ('until', 44), ('hold', 'none'),
             ('until', 110)],
    # a room with statics and enemies: walk and turn
    'room': [('poke', 'dbg_god', 1), ('until', 2), ('warp', 33, 14, 0), ('until', 5), ('hold', 'up'), ('until', 12),
             ('hold', 'right'), ('until', 20), ('hold', 'up'), ('until', 28), ('hold', 'left'), ('until', 36),
             ('hold', 'none'), ('until', 38)],
    # the secret room of E1M1: push wall (30,22), items behind it
    'secret': [('poke', 'dbg_god', 1), ('until', 2), ('warp', 31, 22, 3), ('until', 4), ('hold', '2'), ('until', 12),
               ('hold', 'none'), ('until', 16), ('hold', 'up'), ('until', 30), ('hold', 'none'), ('until', 34)],
    # all ten levels: loading, every kind of spawn, idling AI
    'levels': [('poke', 'dbg_god', 1)] + sum([[('until', 2 + 7 * i), ('map', i + 1)] for i in range(10)], []) + [('until', 74)],
}
