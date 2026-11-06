pass                                                                                        
import argparse,collections,copy,json,os,shutil,subprocess,sys,threading,time,webbrowser
from datetime import datetime,timezone
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT=Path(__file__).resolve().parents[1]
def now():return int(time.time()*1000)
def atomic_json(path,value):
    temp=path.with_suffix(path.suffix+'.tmp');temp.write_text(json.dumps(value,allow_nan=False),encoding='utf-8');os.replace(temp,path)

def demo_snapshot(step):
                                                                                
    records=[]
    for i in range(6):
        base=[.52,.38,.65,.45,.55,.40][i];phase=step%30
        shift=0
        if i==0 and 10<=phase<=12:shift=-.12
        if i==1 and 10<=phase<=12:shift=.12
        if i==2 and phase==10:shift=-.15
        if i==3 and phase>=10:shift=-.12
        mid=base+shift+[0,.001,-.001][step%3];width=.12 if i==4 else .02
        def side(best):return [[f'{best-k*.01:.4f}',str(200+k*100)+'.00'] for k in range(5)]
        stamp=now();ticker=f'SIM-{i+1}'
        records.append({'market':{'ticker':ticker,'title':['Brief fall','Brief rise','Isolated outlier','Persistent repricing','Wide spread','Independent forecast example'][i],
            'event_ticker':f'SIM-E{i}','market_type':'binary','notional_value_dollars':'1.0000','status':'active','close_time':'2099-01-01T00:00:00Z',
            'rules_primary':'Invented binary demonstration. No actual Kalshi market.', 'volume_24h_fp':'1000.00'},
            'event':{'event_ticker':f'SIM-E{i}','series_ticker':'SIM','mutually_exclusive':False},
            'series':{'fee_type':'quadratic','fee_multiplier':1,'category':'Simulated'},
            'orderbook':{'orderbook_fp':{'yes_dollars':side(mid-width/2),'no_dollars':side(1-mid-width/2)}},'received_ms':stamp,'errors':[]})
    return {'schema':'kalshilens.snapshot.v1','synthetic':True,'source':'Invented monitor demo','records':records,'coverage':{'selected':6,'discovered':6,'selection':'Synthetic sequence'}}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=Path,default=ROOT/'bin/kalshilens.exe')
    ap.add_argument('--series',default='');ap.add_argument('--event',default='');ap.add_argument('--ticker',default='')
    ap.add_argument('--max-markets',type=int,default=8);ap.add_argument('--pages',type=int,default=1)
    ap.add_argument('--interval',type=float,default=15);ap.add_argument('--port',type=int,default=8765)
    ap.add_argument('--config',type=Path,default=ROOT/'monitor_config.json');ap.add_argument('--out',type=Path)
    ap.add_argument('--demo',action='store_true');ap.add_argument('--no-browser',action='store_true')
    ap.add_argument('--cycles',type=int,default=0,help='0 means continue until Ctrl+C')
    ap.add_argument('--retain',type=int,default=120)
    args=ap.parse_args();args.exe=args.exe.resolve();args.config=args.config.resolve()
    if not args.exe.is_file():ap.error('Build the C++ executable first using build_windows.cmd')
    if not args.config.is_file():ap.error('Monitor config is missing')
    if not 1<=args.max_markets<=100 or not 1<=args.pages<=20:ap.error('Invalid scan limits')
    if not 1024<=args.port<=65535 or args.interval<(1 if args.demo else 10) or args.retain<2 or args.cycles<0:ap.error('Invalid scheduler settings')
    if args.ticker and (args.series or args.event):ap.error('Use ticker alone, or series/event')
    session=args.out.resolve() if args.out else ROOT/'runs'/('monitor_'+str(now()))
    session.mkdir(parents=True,exist_ok=False)
    view={'status':'starting','message':'Collecting first snapshot…','last_success_ms':None,'data':None};lock=threading.Lock();stop=threading.Event()
    html=(ROOT/'web/monitor.html').read_bytes()
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            host=self.headers.get('Host','')
            if host not in (f'127.0.0.1:{args.port}',f'localhost:{args.port}'):
                self.send_error(403);return
            path=urlsplit(self.path).path
            if path=='/':body=html;kind='text/html; charset=utf-8'
            elif path=='/api/latest':
                with lock:body=json.dumps(view,allow_nan=False).encode()
                kind='application/json'
            else:self.send_error(404);return
            self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Content-Length',str(len(body)));self.end_headers()
            try:self.wfile.write(body)
            except (BrokenPipeError,ConnectionResetError):pass
        def log_message(self,*args):pass
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    url=f'http://127.0.0.1:{args.port}/';print('Dashboard:',url,flush=True);print('Records:',session,flush=True)
    if not args.no_browser:webbrowser.open(url)
    def update(**items):
        with lock:view.update(items)
    def execute(cmd,log,timeout):
        with log.open('w',encoding='utf-8') as f:
            p=subprocess.Popen([str(x) for x in cmd],stdout=f,stderr=subprocess.STDOUT,cwd=ROOT)
            try:code=p.wait(timeout=timeout)
            except BaseException:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait()
                raise
        if code:raise RuntimeError(log.read_text(encoding='utf-8',errors='replace')[-1500:])
    state=session/'state.json';attempt=0;failures=0;captures=collections.deque()
    try:
        while not stop.is_set() and (args.cycles==0 or attempt<args.cycles):
            attempt+=1;capture=session/f'capture_{attempt:06d}';log=session/'last_capture.log'
            update(status='collecting',message=f'Collecting snapshot {attempt}; previous quotes retain their original timestamps.')
            try:
                                                                                                        
                cfg=json.loads(args.config.read_text(encoding='utf-8'));atomic_json(session/'cycle_config.json',cfg)
                if args.demo:
                    capture.mkdir();snapshot=demo_snapshot(attempt-1);atomic_json(capture/'snapshot.json',snapshot)
                                                                                              
                    cfg['min_span_seconds']=1
                    if 'SIM-6' not in cfg.get('forecasts',{}):cfg.setdefault('forecasts',{})['SIM-6']={'probability':.55,'lower':.50,'upper':.60,'source':'Invented demo estimate','as_of_ms':now()-1000,'valid_until_ms':now()+60000}
                    atomic_json(session/'cycle_config.json',cfg)
                else:
                    cmd=[args.exe,'scan','--max-markets',args.max_markets,'--pages',args.pages,'--out',capture]
                    for key in ('series','event','ticker'):
                        if getattr(args,key):cmd += ['--'+key,getattr(args,key)]
                    execute(cmd,log,min(1800,60+args.max_markets*45))
                out=session/'next.json';newstate=session/'next_state.json'
                if state.exists():shutil.copy2(state,newstate)
                elif newstate.exists():newstate.unlink()
                execute([args.exe,'anomaly','--in',capture/'snapshot.json','--state',newstate,'--config',session/'cycle_config.json','--out',out],session/'last_analysis.log',60)
                data=json.loads(out.read_text());os.replace(newstate,state);atomic_json(session/'latest.json',data)
                atomic_json(capture/'monitor.json',data);atomic_json(capture/'monitor_config.json',cfg)
                valid=any(r['monitor_status']!='quality_blocked' for r in data['markets'])
                update(status='ok' if valid else 'no_usable_quotes',message='Waiting for the next capture.' if valid else 'No usable current quotes. Inspect quality reasons.',data=data,last_success_ms=now())
                failures=0;print(f"Capture {attempt}: {sum(r['candidate'] for r in data['markets'])} candidates; {len(data['markets'])} markets",flush=True)
            except Exception as e:
                failures+=1;update(status='error',message=str(e),last_error_ms=now())
                print('Capture failed:',e,flush=True)
            if capture.exists():captures.append(capture)
            while len(captures)>args.retain:shutil.rmtree(captures.popleft())
            pause=min(120,args.interval*2**min(failures,3))
            if not args.cycles or attempt<args.cycles:stop.wait(pause)
        update(status='stopped',message='Capture limit reached. Restart to collect more quotes.')
    except KeyboardInterrupt:print('\nMonitor stopped. Completed records are saved.',flush=True)
    finally:
        server.shutdown();server.server_close()
if __name__=='__main__':main()
