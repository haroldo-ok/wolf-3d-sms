"""ROM data packer: blobs -> 16 KiB banks addressed through slot 2 (0x8000)."""
import os
BANK = 0x4000
class Packer:
    def __init__(self, first_bank):
        self.first = first_bank
        self.banks = []         # list of bytearray
        self.syms = []          # (name, bank, addr, size)
    def _new(self):
        self.banks.append(bytearray())
        return len(self.banks) - 1
    def add(self, name, data, align=1, whole_bank=False):
        data = bytes(data)
        assert len(data) <= BANK, (name, len(data))
        # first fit
        pick = None
        if not whole_bank:
            for i, b in enumerate(self.banks):
                pad = (-len(b)) % align
                if len(b) + pad + len(data) <= BANK:
                    pick = i; break
        if pick is None:
            pick = self._new()
        b = self.banks[pick]
        b += bytes((-len(b)) % align)
        addr = 0x8000 + len(b)
        b += data
        self.syms.append((name, self.first + pick, addr, len(data)))
        return self.first + pick, addr
    def add_raw_bank(self, name, data):
        """a whole bank of position dependent data (caller knows the base)"""
        assert len(data) <= BANK, (name, len(data))
        i = self._new()
        self.banks[i] += bytes(data)
        self.syms.append((name, self.first + i, 0, len(data)))
        return self.first + i
    def add_split(self, name, data, chunk):
        """big data split into whole-bank chunks of `chunk` bytes, each starting a new bank"""
        out = []
        for i in range(0, len(data), chunk):
            bank = self._new()
            self.banks[bank] += data[i:i+chunk]
            out.append(self.first + bank)
        self.syms.append((name, out[0], 0x8000, len(data)))
        return out
    def write(self, binpath):
        with open(binpath, 'wb') as f:
            for b in self.banks:
                f.write(bytes(b) + b'\xff' * (BANK - len(b)))
        return len(self.banks)
