"""Where the converters find the id data: the shareware data files (*.WL1), id's source release (the names of the
graphics and sounds, the palette) and, for tools/port_game.py only, the Genesis port.  The first that exists of:
the environment (WL_DATA, WL_SRC, WL_MD), ../work/... next to the project (what tools/fetch_data.sh makes), work/...
inside it."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def pick(env, *cands):
    if os.environ.get(env):
        return os.environ[env]
    for c in cands:
        if os.path.isdir(c):
            return c
    return cands[0]


DATA = pick('WL_DATA', os.path.join(ROOT, '..', 'work', 'data'), os.path.join(ROOT, 'work', 'data'))
SRC = pick('WL_SRC', os.path.join(ROOT, '..', 'work', 'wolf3d'), os.path.join(ROOT, 'work', 'wolf3d'))
MD = pick('WL_MD', os.path.join(ROOT, '..', 'wolf-3d-md', 'src'), os.path.join(ROOT, 'wolf-3d-md', 'src'))
