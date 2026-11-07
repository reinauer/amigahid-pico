#!/usr/bin/env python3
"""Extract dated LCSC stock data from saved public product pages.

Input: review/lcsc-stock/C*.html, fetched directly from each product URL.
No cookies, account data, or cart interaction is needed.
"""
from pathlib import Path
import re,json,datetime,hashlib
R=Path(__file__).resolve().parents[1];parts=json.loads((R/'circuit.json').read_text())['parts'];rows=[]
for code in sorted({p['lcsc'] for p in parts if p.get('lcsc')}):
 f=R/'review/lcsc-stock'/(code+'.html');s=f.read_text();m=re.search(r'<script id="__NEXT_DATA__"[^>]*>(.*?)</script>',s);assert m,code
 d=json.loads(m[1])['props']['pageProps']['webData'];assert d['productCode']==code
 refs=[p['ref'] for p in parts if p.get('lcsc')==code]
 r={k:d.get(k) for k in ['productCode','productModel','brandNameEn','stockNumber','overseasStockVO','minBuyNumber','minPacketNumber','productPriceList','isForeignOnsale','isPreSale','productArrange']}
 r.update(refs=refs,quantity_per_board=len(refs),url='https://www.lcsc.com/product-detail/'+code+'.html',checked_utc=datetime.datetime.fromtimestamp(f.stat().st_mtime,datetime.timezone.utc).isoformat(),source_sha256=hashlib.sha256(f.read_bytes()).hexdigest());rows.append(r)
 print(code,','.join(refs),d['productModel'],'stock',d.get('stockNumber'),'immediate',(d.get('overseasStockVO') or {}).get('shipImmediately'),'onsale',d.get('isForeignOnsale'))
(R/'stock-check.json').write_text(json.dumps(rows,indent=2)+'\n')
