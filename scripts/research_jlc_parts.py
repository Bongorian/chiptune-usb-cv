"""Read public JLC part pages and save auditable component metadata."""
import concurrent.futures
import json
import re
import urllib.request
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

OUT = Path(__file__).resolve().parents[1] / 'research/jlc-review'
OUT.mkdir(exist_ok=True)
PARTS = ['C20526', 'C12624', 'C965804', 'C72038', 'C2297', 'C2827654',
         'C558442', 'C97521', 'C179173', 'C2827718', 'C541401', 'C9002',
         'C25190', 'C72043', 'C83935', 'C94514']


def fetch(code):
    url = 'https://jlcpcb.com/partdetail/' + code
    try:
        html = urllib.request.urlopen(urllib.request.Request(url, headers={
            'User-Agent': 'Mozilla/5.0'}), timeout=25).read().decode()
        (OUT / (code + '-jlc.html')).write_text(html)
        text = ''.join(json.loads(m[1]) for m in re.finditer(
            r'self\.__next_f\.push\(\[1,("(?:[^"\\]|\\.)*")\]\)', html))
        objects = []
        for match in re.finditer(r'"componentInfo":', text):
            try:
                data = json.JSONDecoder().raw_decode(text[match.end():])[0]
                if isinstance(data, dict) and data.get('componentCode') == code:
                    objects.append(data)
            except (ValueError, TypeError):
                pass
        result = {}
        for obj in objects:
            result.update(obj)
        assert result.get('componentCode') == code, 'No part metadata'
        result['fetched_at'] = datetime.now(ZoneInfo('Asia/Tokyo')).isoformat()
        result['source_url'] = url
        (OUT / (code + '-jlc.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2))
        return {key: result.get(key) for key in ['componentCode', 'componentModelEn',
            'componentSpecificationEn', 'componentLibraryType', 'preferredComponentFlag',
            'overseasStockCount', 'lossNumber', 'leastPatchNumber', 'initialPrice']}
    except Exception as exc:
        return {'code': code, 'error': str(exc)}


if __name__ == '__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        for result in pool.map(fetch, PARTS):
            print(json.dumps(result, ensure_ascii=False), flush=True)
