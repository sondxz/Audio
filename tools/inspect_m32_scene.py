"""Read-only SCN text inventory. No OSC, mixer access or scene conversion.

All unknown fields and numeric source IDs remain raw, avoiding undocumented
firmware-specific enum guesses. The input is data and is never executed.
"""
import hashlib
import json
from pathlib import Path
import re
import shlex
import sys


def inspect_scene(path):
    raw = Path(path).read_bytes()
    if len(raw) > 10 * 1024 * 1024:
        raise ValueError("Scene exceeds 10 MB")
    lines = raw.decode('utf-8-sig').splitlines()
    if not lines or not re.match(r'^#\d+\.\d+#', lines[0]):
        raise ValueError("Missing M32/X32 scene header")
    header = shlex.split(lines[0])
    entries = {}
    for line_number, line in enumerate(lines[1:], 2):
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        tokens = shlex.split(line, comments=False)
        if not tokens or not tokens[0].startswith('/'):
            raise ValueError(f"Invalid scene record on line {line_number}")
        address = tokens[0]
        if address in entries:
            raise ValueError(f"Duplicate scene address {address}")
        entries[address] = {'line': line_number, 'values': tokens[1:]}
    return {'file': str(Path(path).resolve()), 'bytes': len(raw),
            'sha256': hashlib.sha256(raw).hexdigest(), 'header': header,
            'records': len(entries), 'entries': entries}


def write_report(scene, folder):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    entries = scene['entries']
    name = Path(scene['file']).stem
    def field(address):
        return entries.get(address, {}).get('values', [])
    report = [f'# Scene M32R: {name}', '',
              'Đọc file offline; chưa nạp vào mixer hoặc chuyển đổi sang scene LiveAudioHost.', '',
              f"SHA256: `{scene['sha256']}`; {scene['bytes']} bytes; {scene['records']} bản ghi.", '',
              '## Routing lưu trong file', '', '| Địa chỉ | Giá trị |', '|---|---|']
    for address in entries:
        if address.startswith('/config/routing'):
            report.append(f"| `{address}` | `{' '.join(field(address))}` |")
    report += ['', '## Matrix', '', '| Matrix | Tên | Mix lưu nguyên bản | Dynamics |', '|---|---|---|---|']
    for index in range(1, 7):
        prefix = f'/mtx/{index:02d}'
        config = field(prefix + '/config')
        report.append(f"| {index} | {config[0] if config else ''} | `{' '.join(field(prefix + '/mix'))}` | `{' '.join(field(prefix + '/dyn')[:3])}` |")
    report += ['', '## Output', '', '| Output | Source ID / tap / polarity lưu nguyên bản | Delay ON/OFF và ms |', '|---|---|---|']
    for index in range(1, 17):
        prefix = f'/outputs/main/{index:02d}'
        report.append(f"| {index} | `{' '.join(field(prefix))}` | `{' '.join(field(prefix + '/delay'))}` |")
    report += ['', '## Đối chiếu tiếp theo', '',
               '- REC input blocks: AN1-8, AN9-16, CARD1-8, A9-16; PLAY lưu CARD1-32. Kiểm tra chế độ thực trên mixer.',
               '- CARD output block 17-24 lưu CARD1-8. Cần vẽ đường send/return qua host và kiểm tra vòng lặp trước bật audio.',
               '- Source ID output giữ số gốc; cần đối chiếu đúng enum/firmware bằng M32-Edit hoặc mixer trước gán tên.',
               '- Dynamics matrix trong file OFF; chưa có căn cứ đánh giá limiter bảo vệ loa phía ngoài mixer.',
               '- Delay trong scene là tham số đã đặt, không phải latency USB/ASIO hay kết quả đo alignment hiện tại.',
               '- Chưa xác minh HA gain/phantom với thiết bị thật; xem dữ liệu raw JSON nếu cần đối chiếu.',
               '- Chưa có importer .scn vào LiveAudioHost: kiến trúc bus/matrix và DSP hai ứng dụng chưa tương đương.']
    (folder / f'{name}-scene-inventory.json').write_text(json.dumps(scene, ensure_ascii=False, indent=2), encoding='utf-8')
    (folder / f'{name}-scene-review.md').write_text('\n'.join(report) + '\n', encoding='utf-8')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: inspect_m32_scene.py input.scn report-directory')
    data = inspect_scene(sys.argv[1])
    write_report(data, sys.argv[2])
    print(f"Read {data['records']} scene records; input unchanged; SHA256 {data['sha256']}")
