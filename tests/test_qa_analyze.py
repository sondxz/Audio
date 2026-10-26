import sys
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from qa_analyze import assess


def row(h=0, **changes):
    base=dict(timestamp_utc=(datetime(2026,10,22,tzinfo=timezone.utc)+timedelta(hours=h)).isoformat(),
              driver='ASIO',device='Lab interface',sample_rate='48000',buffer_samples='128',
              input_channels='2',output_channels='2',muted='0',safety_locked='0',
              device_generation='1',callback_count=str(int(1000+h*3600*375)),
              deadline_overruns='0',consecutive_overruns='0',max_callback_ms='1.0',
              juce_xruns='0',device_xruns='0',device_errors='0',cpu_percent='2.0')
    return {**base,**changes}


class QaAcceptanceTests(unittest.TestCase):
    def test_pristine_24_hours(self):
        self.assertEqual(assess([row(s/3600) for s in range(0,86401,5)],24.0),([],[]))
    def test_two_records_do_not_prove_24h_continuity(self):
        flags,_=assess([row(0),row(24)],24.0)
        self.assertTrue(any('monitoring gap' in f for f in flags))
    def test_sustained_overrun_rejected(self):
        flags,_=assess([row(0),row(24,deadline_overruns='9')],24.0)
        self.assertTrue(any('deadline' in f for f in flags))
    def test_driver_disconnection_rejected(self):
        flags,_=assess([row(0),row(24,device_generation='2')],24.0)
        self.assertTrue(any('Device disconnected' in f for f in flags))
    def test_short_session_incomplete(self):
        flags,_=assess([row(0),row(1)],24.0)
        self.assertTrue(any('Duration' in f for f in flags))
    def test_unknown_driver_xruns_inconclusive(self):
        flags,unknown=assess([row(0,device_xruns='-1'),row(1/3600,device_xruns='-1')],1/3600)
        self.assertFalse(flags)
        self.assertTrue(any('unsupported' in f for f in unknown))
    def test_juce_fallback_overrun_rejected(self):
        flags,_=assess([row(0,juce_xruns='0'),row(24,juce_xruns='3')],24.0)
        self.assertTrue(any('JUCE' in f for f in flags))
    def test_device_native_xruns_rejected(self):
        flags,_=assess([row(0,device_xruns='0'),row(24,device_xruns='2')],24.0)
        self.assertTrue(any('Native device' in f for f in flags))

    def test_muted_not_accepted(self):
        _,unknown=assess([row(0,muted='1'),row(24,muted='1')],24.0)
        self.assertTrue(any('muted' in f for f in unknown))

    def test_invalid_numeric_records(self):
        for key in ('sample_rate','device_errors','cpu_percent','callback_count','device_xruns'):
            for invalid in ('nan','inf','-inf','garbage'):
                with self.subTest(key=key,value=invalid):
                    flags,_=assess([row(0),row(1/3600,**{key:invalid})],1/3600)
                    self.assertTrue(any('Malformed' in f for f in flags))
        for key in ('device_errors','input_channels','callback_count','max_callback_ms','cpu_percent'):
            flags,_=assess([row(0),row(1/3600,**{key:'-2'})],1/3600)
            self.assertTrue(flags)

    def test_middle_timestamp_must_be_monotonic(self):
        flags,_=assess([row(0),row(2/3600),row(1/3600),row(3/3600)],3/3600)
        self.assertTrue(any('monotonic' in f for f in flags))

    def test_stalled_callback_interval_rejected(self):
        flags,_=assess([row(0),row(1/3600,callback_count='1000'),row(2/3600)],2/3600)
        self.assertTrue(any('callback progress' in f for f in flags))

    def test_partial_mute_is_not_continuous_operational_workload(self):
        flags,unknown=assess([row(0),row(1/3600,muted='1')],1/3600)
        self.assertFalse(flags)
        self.assertTrue(unknown)

    def test_naive_timestamp_rejected(self):
        flags,_=assess([row(0,timestamp_utc='2026-10-22T00:00:00'),row(1/3600)],1/3600)
        self.assertTrue(any('Invalid timestamps' in f for f in flags))

    def test_duration_must_be_finite(self):
        for hours in (float('nan'),float('inf'),0,-1):
            flags,_=assess([row(0),row(1/3600)],hours)
            self.assertTrue(flags)


if __name__=='__main__':unittest.main()
