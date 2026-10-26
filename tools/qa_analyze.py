#!/usr/bin/env python3
"""Review GUI-generated LiveAudioHost QA CSV. No device or audio access.

Exit 0 only if the requested minimum duration and available checks pass.
Unknown/unreported driver XRuns make driver-level acceptance INCONCLUSIVE.
Never equate processor deadline overruns with driver XRuns.
"""
import argparse
import csv
import math
from datetime import datetime
from pathlib import Path
import sys


def num(row, key):
    try:
        value = float(row[key])
        if not math.isfinite(value):
            raise ValueError()
        return value
    except (KeyError, ValueError, TypeError):
        raise ValueError(f"Invalid or absent field {key}")


def timestamps(rows):
    parsed = [datetime.fromisoformat(r['timestamp_utc'].strip().replace('Z', '+00:00')) for r in rows]
    if any(t.tzinfo is None for t in parsed):
        raise ValueError('timestamps must include a UTC offset')
    return parsed


def assess(rows, min_hours, max_gap_seconds=5.0):
    flags = []
    unknown = []
    if not math.isfinite(min_hours) or min_hours <= 0:
        return ['Minimum duration must be finite and positive'], []
    if not math.isfinite(max_gap_seconds) or max_gap_seconds <= 0:
        return ['Maximum log gap must be finite and positive'], []
    if len(rows) < 2:
        return ["Insufficient QA log samples"], []
    try:
        times = timestamps(rows)
        intervals = [(b-a).total_seconds() for a,b in zip(times,times[1:])]
        seconds = (times[-1]-times[0]).total_seconds()
    except (KeyError, ValueError, TypeError) as exc:
        return [f"Invalid timestamps: {exc}"], []
    if seconds < min_hours * 3600:
        flags.append(f"Duration {seconds:.1f}s is below {min_hours:.2f} hours")
    if any(dt <= 0 for dt in intervals):
        flags.append("Time not monotonic")
    if any(dt > max_gap_seconds for dt in intervals):
        flags.append(f"QA monitoring gap exceeds {max_gap_seconds:g}s; continuity unverified")
    try:
        integer_fields = ('device_errors','deadline_overruns','device_generation','callback_count',
                          'consecutive_overruns','input_channels','output_channels','buffer_samples')
        for r in rows:
            for k in integer_fields:
                v = num(r,k)
                if v < 0 or not v.is_integer():
                    raise ValueError(f'{k} must be a nonnegative integer')
            for k in ('muted','safety_locked'):
                if num(r,k) not in (0,1):
                    raise ValueError(f'{k} must be 0 or 1')
            for k in ('juce_xruns','device_xruns'):
                v = num(r,k)
                if v < -1 or not v.is_integer():
                    raise ValueError(f'{k} must be -1 or a nonnegative integer')
            for k in ('cpu_percent','max_callback_ms'):
                if num(r,k) < 0:
                    raise ValueError(f'{k} must be nonnegative')
            if not 8000 <= num(r,'sample_rate') <= 384000:
                raise ValueError('sample_rate outside supported range')
            if num(r,'buffer_samples') <= 0:
                raise ValueError('buffer_samples must be positive')
            if num(r,'input_channels') <= 0 or num(r,'output_channels') <= 0:
                raise ValueError('No active input/output channels')
        for k in ("device_errors", "deadline_overruns"):
            values = [num(r,k) for r in rows]
            if values[-1] > values[0]:
                flags.append(f"{k} increased from {values[0]:g} to {values[-1]:g}")
            if any(b<a for a,b in zip(values,values[1:])):
                flags.append(f"{k} counter reset; device/process restarted?")
        if any(num(r,'safety_locked') > 0 for r in rows):
            flags.append("Safety interlock was latched")
        gens = {num(r,'device_generation') for r in rows}
        if len(gens)>1:flags.append("Device disconnected/restarted or callback reattached during capture")
        drivers={r['driver'] for r in rows};devices={r['device'] for r in rows}
        if len(drivers)!=1 or len(devices)!=1 or 'none' in devices:
            flags.append("Audio device changed or was unavailable")
        rates={num(r,'sample_rate') for r in rows}
        buffers={num(r,'buffer_samples') for r in rows}
        if any(len({num(r,k) for r in rows})!=1 for k in ('input_channels','output_channels')):
            flags.append('Active audio channel counts changed')
        if len(rates)!=1 or 0 in rates:flags.append("Sample rate missing or changed")
        if len(buffers)!=1 or 0 in buffers:flags.append("Buffer missing or changed")
        callback_counts=[num(r,'callback_count') for r in rows]
        if callback_counts[-1]<=callback_counts[0]:flags.append("No audio callbacks observed")
        for i,dt in enumerate(intervals):
            actual = callback_counts[i+1]-callback_counts[i]
            expected = dt*num(rows[i],'sample_rate')/num(rows[i],'buffer_samples')
            if dt > 0 and abs(actual-expected) > max(8.0, 0.1*expected):
                flags.append('Audio callback progress inconsistent with elapsed time/rate/buffer')
                break
        # AudioDeviceManager may return an ESTIMATE when the native device does
        # not support XRun counts. Never label the manager's figure 'hardware reported'.
        juce_xruns=[num(r,'juce_xruns') for r in rows]
        if any(x<0 for x in juce_xruns):
            unknown.append('JUCE XRun counter unsupported or unavailable')
        if juce_xruns[-1]>juce_xruns[0]:
            flags.append(f"JUCE (native-or-estimated) XRuns increased by {juce_xruns[-1]-juce_xruns[0]:g}")
        elif any(b<a for a,b in zip(juce_xruns,juce_xruns[1:])):
            flags.append("JUCE XRun counter reset")
        native_xruns=[num(r,'device_xruns') for r in rows]
        if any(x<0 for x in native_xruns):
            unknown.append("Native device XRun counter unsupported or unavailable")
        elif native_xruns[-1]>native_xruns[0]:
            flags.append(f"Native device XRuns increased by {native_xruns[-1]-native_xruns[0]:g}")
        elif any(b<a for a,b in zip(native_xruns,native_xruns[1:])):
            flags.append("Native device XRun counter reset")
        if any(num(r,'muted')>0 for r in rows):
            unknown.append("Audio was muted: continuous operational workload was not verified")
    except (KeyError, ValueError, TypeError) as exc:
        flags.append(f"Malformed QA record: {exc}")
    return flags, unknown


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv_file',type=Path)
    parser.add_argument('--min-hours',type=float,default=24.0,
        help='Minimum uninterrupted duration (default 24 hours)')
    parser.add_argument('--max-gap-seconds',type=float,default=5.0,
        help='Largest accepted interval between 1 Hz GUI records (default 5 seconds)')
    args=parser.parse_args(argv)
    if not math.isfinite(args.min_hours) or args.min_hours<=0:
        parser.error('--min-hours must be positive')
    if not math.isfinite(args.max_gap_seconds) or args.max_gap_seconds<=0:
        parser.error('--max-gap-seconds must be finite and positive')
    try:
        with args.csv_file.open(encoding='utf-8-sig',newline='') as f:
            records=list(csv.DictReader(f))
        flags,unknown=assess(records,args.min_hours,args.max_gap_seconds)
    except (OSError, UnicodeError, csv.Error) as exc:
        print(f"INVALID INPUT: {exc}");return 2
    print(f"Samples: {len(records)} | Requested min duration: {args.min_hours:g}h")
    for issue in flags:print('FAIL:',issue)
    for issue in unknown:print('INCONCLUSIVE:',issue)
    if flags:
        print('STATUS: FAIL / NOT ACCEPTED');return 1
    if unknown:
        print('STATUS: INCONCLUSIVE / NOT ACCEPTED');return 3
    print('STATUS: AUTOMATED LOG CRITERIA MET (not hardware calibration or show certification)')
    return 0


if __name__=='__main__':sys.exit(main())
