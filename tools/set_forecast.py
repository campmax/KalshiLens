"""Record a sourced probability estimate, interval and expiry for the next monitor cycle."""
import argparse,json,os,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('ticker');p.add_argument('--probability',type=float,required=True);p.add_argument('--lower',type=float,required=True);p.add_argument('--upper',type=float,required=True);p.add_argument('--source',required=True);p.add_argument('--minutes',type=float,default=15);p.add_argument('--config',type=Path,default=Path(__file__).resolve().parents[1]/'monitor_config.json')
a=p.parse_args()
if not 0<=a.lower<=a.probability<=a.upper<=1 or not 0<a.minutes<=1440 or not a.source.strip():p.error('Use 0 <= lower <= probability <= upper <= 1, nonempty source and expiry <= 1440 minutes')
c=json.loads(a.config.read_text(encoding='utf-8'));t=int(time.time()*1000)
c.setdefault('forecasts',{})[a.ticker]={'probability':a.probability,'lower':a.lower,'upper':a.upper,'source':a.source,'as_of_ms':t,'valid_until_ms':t+int(a.minutes*60000)}
temp=a.config.with_suffix('.json.tmp');temp.write_text(json.dumps(c,indent=2,allow_nan=False),encoding='utf-8');os.replace(temp,a.config)
print('Estimate saved. The next completed monitor cycle will use it. This tool does not validate forecasting skill.')
