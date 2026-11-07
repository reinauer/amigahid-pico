#!/usr/bin/env python3
"""Extract public assembly stock from archived JLCPCB product HTML.

No account or cart is accessed. Public supply stock and pre-order quantities
are kept distinct; a quote must confirm allocation and attrition for its batch.
"""
from pathlib import Path
import datetime
import hashlib
import html
import json
import re

R = Path(__file__).resolve().parents[1]
parts = json.loads((R / 'circuit.json').read_text())['parts']
out = []
for code in sorted({p['lcsc'] for p in parts if p.get('lcsc')}):
    f = R / 'review/jlc-stock' / (code + '.html')
    raw = f.read_text()
    chunks = []
    for m in re.finditer(r'<script>self\.__next_f\.push\((.*?)\)</script>', raw):
        a = json.loads(m[1])
        if len(a) > 1 and isinstance(a[1], str):
            chunks.append(a[1])
    records = []
    for line in ''.join(chunks).splitlines():
        try:
            d = json.loads(line.split(':', 1)[1])
        except (ValueError, IndexError):
            continue
        if isinstance(d, dict) and d.get('componentCode') == code and 'overseasStockCount' in d:
            records.append(d)
    assert len(records) == 1, (code, len(records))
    d = records[0]
    text = html.unescape(re.sub('<[^>]+>', ' ', re.sub(r'<script.*?</script>', '', raw, flags=re.S)))
    text = ' '.join(text.split())
    assembly = re.search(r'Assembly Type (.*?) PCBA Type (.*?) Note:', text)
    assert assembly, code
    row = {k: d.get(k) for k in ['componentCode', 'componentModelEn',
           'overseasStockCount', 'canPresaleNumber', 'preMinPurchaseNum',
           'leastPatchNumber', 'lossNumber', 'initialPrice']}
    row.update(refs=[p['ref'] for p in parts if p.get('lcsc') == code],
               assembly_type=assembly[1], pcba_type=assembly[2].split(' MSL Level', 1)[0],
               url='https://jlcpcb.com/partdetail/' + code,
               checked_utc=datetime.datetime.fromtimestamp(f.stat().st_mtime, datetime.timezone.utc).isoformat(),
               source_sha256=hashlib.sha256(f.read_bytes()).hexdigest())
    out.append(row)
    print(code, d['overseasStockCount'], assembly[1], assembly[2])
(R / 'assembly-stock-check.json').write_text(json.dumps(out, indent=2) + '\n')
