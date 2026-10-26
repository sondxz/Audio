#!/usr/bin/env python3
"""Third-party scipy.signal.firwin Blackman-window cross-check for C++ FIR taps."""
import io
import subprocess
import sys
import numpy as np
from scipy import signal

def main(binary):
    for rate in (44100, 48000, 96000):
        for taps in (129, 257, 513, 1025, 2049):
            for kind in ('lp', 'hp'):
                cutoff=3000
                output=subprocess.check_output([binary, kind, str(rate),str(cutoff),str(taps)],text=True)
                rows=np.loadtxt(io.StringIO(output),delimiter=',')
                actual=rows[:,1]
                expected=signal.firwin(taps,cutoff,window='blackman',pass_zero=(kind=='lp'),fs=rate,scale=True)
                error=float(np.max(np.abs(actual-expected)))
                if error>2e-8: raise AssertionError((rate,taps,kind,error))
                print(f'{kind} fs={rate} taps={taps}: abs max difference vs SciPy firwin = {error:.3e}')
    print('PASS independent SciPy FIR coefficient check')

if __name__=='__main__':
    main(sys.argv[1] if len(sys.argv)>1 else './build-core/lah_fir_cli')
