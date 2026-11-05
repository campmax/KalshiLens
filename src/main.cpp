#include "client.hpp"
#include "dashboard.hpp"
#include "anomaly.hpp"
#ifdef _WIN32
#include <shellapi.h>
#pragma comment(lib,"shell32.lib")
#endif
using namespace kl;
static void writeText(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);need(bool(f),"Cannot write "+p.string());f<<text;need(bool(f),"Write failed "+p.string());}
static std::string csvCell(const J& j){std::string s=j.is_null()?"":j.is_string()?j.get<std::string>():j.dump();
 if(!s.empty()&&j.is_string()&&(s[0]=='='||s[0]=='+'||s[0]=='-'||s[0]=='@'||s[0]=='\t'||s[0]=='\r'))s="'"+s;
 std::string o="\"";for(char c:s){if(c=='\"')o+="\"\"";else o+=c;}return o+"\"";}
static void csv(const fs::path& p,const J& rows,const std::vector<std::string>& fields){std::ostringstream out;
 for(size_t i=0;i<fields.size();++i)out<<(i?",":"")<<fields[i];
 out<<'\n';
 for(const auto& row:rows){for(size_t i=0;i<fields.size();++i)out<<(i?",":"")<<csvCell(row.value(fields[i],J(nullptr)));out<<'\n';}writeText(p,out.str());}
static void report(const fs::path& dir,J a,const J& history){
 a["history"]=history;std::string payload=a.dump(-1,' ',true);                                                            
 size_t pos=0;while((pos=payload.find('<',pos))!=std::string::npos){payload.replace(pos,1,"\\u003c");pos+=6;}
 std::string page=dashboardTemplate;const std::string marker="__KALSHILENS_DATA__";pos=page.find(marker);need(pos!=std::string::npos,"Report template marker missing");page.replace(pos,marker.size(),payload);
 writeText(dir/"report.html",page);writeJson(dir/"analysis.json",a);
 csv(dir/"markets.csv",a["markets"],{"ticker","title","event_ticker","category","status","mid","spread","yes_bid","yes_ask","no_bid","no_ask","volume_24h","open_interest","imbalance","valid","stale","captured_ms"});
 J fills=J::array();for(const auto& r:a["markets"])if(r.contains("sizes"))for(const auto& size:r["sizes"])for(const auto& side:{"yes","no"}){
  J f=size[side];f["ticker"]=r["ticker"];f["side"]=side;fills.push_back(f);}
 csv(dir/"execution.csv",fills,{"ticker","side","requested","filled","complete","notional","fee","cost","vwap","break_even","worst_price","levels"});
 std::cout<<"\nReport: "<<fs::absolute(dir/"report.html").string()<<"\nMarkets: "<<a["markets"].size()<<" | structural checks: "<<a["structural_checks"].size()<<'\n';
}
static void openReport(const fs::path& p){
#ifdef _WIN32
 auto result=ShellExecuteW(nullptr,L"open",fs::absolute(p).wstring().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
 if(reinterpret_cast<INT_PTR>(result)<=32)std::cerr<<"Could not launch browser; open the report path manually.\n";
#else
 std::cout<<"Open "<<fs::absolute(p).string()<<" in your browser.\n";
#endif
}
static void help(){std::cout<<R"(KalshiLens 1.0 -- read-only public prediction-market research

kalshilens demo --open
kalshilens anomaly --in snapshot.json --state state.json --config monitor_config.json --out monitor.json
kalshilens scan --series KXHIGHNY --max-markets 24 --open
kalshilens scan --ticker YOUR-MARKET-TICKER --open
kalshilens watch --series KXHIGHNY --samples 5 --interval 30 --open
kalshilens analyze --in runs/example/snapshot.json --config research.json --open
kalshilens compare --before first/snapshot.json --after second/snapshot.json
kalshilens score --in examples/forecasts_demo.json

Options:
 --out PATH          New run directory (existing directories are refused)
 --config PATH       Research assumptions: probability estimates and reviewed relationships
 --quantity 100      Requested contracts in comparisons (supports hundredths)
 --member direct    Balance precision: direct ($0.0001) or fcm ($0.01)
 --series TICKER    Restrict market discovery to a series
 --event TICKER     Restrict discovery to one event
 --ticker TICKER    Analyze one named market
 --pages 2          Scan 1-20 pages of 100 markets; results are not a full-universe ranking
 --max-markets 24   Fetch detailed books for the top 1-100 discovered markets by 24h volume
 --samples 5        Watch captures 2-60 snapshots, then exits
 --interval 30      Watch pause in seconds AFTER each completed capture (minimum 10)
 --delay-ms 300     Minimum API request spacing, at least 250 ms
 --open             Open HTML report using the Windows default browser

No API keys, authenticated account operations, or order entry are implemented.
Missing or unsupported fee metadata suppresses fee-adjusted claims.
)";}
static int integer(const std::string& s,int low,int high){double x=numeric(J(s));need(x>=low&&x<=high&&x==std::floor(x),"Integer option out of range: "+s);return static_cast<int>(x);}
static I quantity(const J& j){double q=numeric(j);need(q>0&&q<=10000000&&std::abs(q*100-std::round(q*100))<1e-6,"Quantity requires 0.01-contract precision");return static_cast<I>(std::llround(q*100));}
int main(int argc,char** argv){try{
 if(argc>1&&std::string(argv[1])=="anomaly")return anomalyCommand(argc,argv);
 std::string command=argc>1?argv[1]:"",in,before,after,out,configPath;CollectOptions opt;Settings s;int samples=5,interval=30,delay=300;bool launch=false;
 if(command.empty()){
  std::cout<<"\nKALSHILENS | Market research\n\n1. Scan live public markets\n2. Open the synthetic demonstration\n3. Show command help\n\nChoose 1, 2 or 3: ";
  std::string choice;std::getline(std::cin,choice);if(choice=="1"){command="scan";std::cout<<"Series ticker (blank for a limited general scan): ";std::getline(std::cin,opt.series);}
  else if(choice=="2")command="demo";else{help();return 0;}
  launch=true;
 }
 if(command=="help"||command=="--help"||command=="-h"){help();return 0;}
 need(command=="scan"||command=="watch"||command=="demo"||command=="analyze"||command=="compare"||command=="score","Unknown command; use help");
 std::map<std::string,std::string> options;
 for(int i=2;i<argc;++i){std::string key=argv[i];if(key=="--open"){launch=true;continue;}need(i+1<argc,"Missing option value");need(!options.count(key),"Duplicate option: "+key);options[key]=argv[++i];}
 if(options.count("--config")){configPath=options["--config"];J config=readJson(configPath);
  need(config.is_object(),"Configuration must be object");std::set<std::string> allowed={"quantity","member","max_skew_seconds","stale_after_seconds","beliefs","relationships"};
  for(auto it=config.begin();it!=config.end();++it)need(allowed.count(it.key())>0,"Unknown configuration field: "+it.key());
  if(config.contains("quantity"))s.quantity=quantity(config["quantity"]);
  std::string member=str(config,"member","direct");need(member=="direct"||member=="fcm","Unknown member precision");s.grid=member=="direct"?100:10000;
  s.maxSkew=num(config,"max_skew_seconds",10);s.staleAfter=num(config,"stale_after_seconds",60);
  s.beliefs=config.value("beliefs",J::object());s.relationships=config.value("relationships",J::array());
 }
 for(auto [key,v]:options){if(key=="--config")continue;else if(key=="--out")out=v;else if(key=="--in")in=v;
  else if(key=="--before")before=v;else if(key=="--after")after=v;else if(key=="--series")opt.series=v;else if(key=="--event")opt.event=v;else if(key=="--ticker")opt.ticker=v;
  else if(key=="--quantity")s.quantity=quantity(J(v));else if(key=="--pages")opt.pages=integer(v,1,20);else if(key=="--max-markets")opt.maxMarkets=integer(v,1,100);
  else if(key=="--samples")samples=integer(v,2,60);else if(key=="--interval")interval=integer(v,10,3600);else if(key=="--delay-ms")delay=integer(v,250,10000);
  else if(key=="--member"){need(v=="direct"||v=="fcm","Member must be direct or fcm");s.grid=v=="direct"?100:10000;}
  else throw std::runtime_error("Unknown option: "+key);
 }
 need(s.maxSkew>=0&&s.maxSkew<=300&&s.staleAfter>=1&&s.staleAfter<=3600,"Invalid freshness settings");
 need(s.beliefs.is_object()&&s.relationships.is_array(),"Invalid beliefs/relationships configuration");
 need(opt.ticker.empty()||(opt.series.empty()&&opt.event.empty()),"Ticker cannot be combined with series/event filters");
 if(out.empty())out="runs/"+command+"_"+std::to_string(nowMs());
 need(!fs::exists(out),"Output directory already exists; choose a new --out path");
 fs::create_directories(out);fs::path dir=out;
 writeJson(dir/"config_used.json",{{"command",command},{"quantity",s.quantity/100.0},{"member",s.grid==100?"direct":"fcm"},
 {"max_skew_seconds",s.maxSkew},{"stale_after_seconds",s.staleAfter},{"beliefs",s.beliefs},{"relationships",s.relationships},
 {"series",opt.series},{"event",opt.event},{"ticker",opt.ticker},{"pages",opt.pages},{"max_markets",opt.maxMarkets},
 {"samples",command=="watch"?samples:1},{"interval_after_capture_seconds",interval},{"delay_ms",delay}});
 if(command=="score"){
  need(!in.empty(),"score requires --in forecasts.json");auto result=calibration(readJson(in));writeJson(dir/"calibration.json",result);
  csv(dir/"calibration_bins.csv",result["bins"],{"low","high","count","predicted","observed","wilson_low","wilson_high"});
  std::cout<<"Forecasts: "<<result["count"]<<" | Brier: "<<result["brier"]<<" | log loss: "<<result["log_loss"]<<"\n"<<result["note"].get<std::string>()<<"\nSaved: "<<fs::absolute(dir).string()<<'\n';return 0;
 }
 if(command=="compare"){
  need(!before.empty()&&!after.empty(),"compare requires --before and --after snapshot files");auto x=readJson(before),y=readJson(after);
  auto result=compare(analyze(x,s),analyze(y,s));writeJson(dir/"comparison.json",result);
  csv(dir/"comparison.csv",result["rows"],{"ticker","before_mid","after_mid","change","before_spread","after_spread","before_captured_ms","after_captured_ms"});
  std::cout<<result["rows"].size()<<" matched markets. Saved "<<fs::absolute(dir/"comparison.csv").string()<<'\n';return 0;
 }
 J history=J::array();int count=command=="watch"?samples:1;
 for(int i=0;i<count;++i){J snapshot;fs::path capture=count>1?dir/("capture_"+std::to_string(i+1)):dir;fs::create_directories(capture);
  if(command=="demo")snapshot=demoSnapshot();else if(command=="analyze"){need(!in.empty(),"analyze requires --in snapshot.json");snapshot=readJson(in);}
  else{std::cout<<"\nReading public Kalshi markets (capture "<<i+1<<"/"<<count<<")...\n";HttpClient client(capture/"raw",delay);
   try{snapshot=collect(opt,[&](const std::string& path){return client.get(path);});}
   catch(...){writeJson(capture/"http_audit.json",client.audit);throw;}
   writeJson(capture/"http_audit.json",client.audit);
  }
  writeJson(capture/"snapshot.json",snapshot);J a=analyze(snapshot,s);
  for(const auto& r:a["markets"])if(r.value("valid",false)&&r.contains("mid")&&!r["mid"].is_null())history.push_back({{"ticker",r["ticker"]},{"mid",r["mid"]},{"time",utc(r["captured_ms"].get<I>())}});
  report(capture,a,history);if(count>1)report(dir,a,history);
  if(launch&&i==0)openReport(dir/"report.html");
  bool anyValid=false;for(const auto& r:a["markets"])anyValid=anyValid||r.value("valid",false);
  need(anyValid,"No valid books available. Diagnostic report and raw capture have been preserved.");
  if(i+1<count){std::cout<<"Waiting "<<interval<<" seconds before next capture. Ctrl+C stops; completed captures remain saved.\n";std::this_thread::sleep_for(std::chrono::seconds(interval));}
 }
 std::cout<<"\nCompleted. No orders or account operations were performed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
