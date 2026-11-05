#include "../src/anomaly.hpp"
#include <iostream>
using namespace kl;
static int passes=0,failures=0;
static void test(const char* name,const std::function<void()>& f){try{f();++passes;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
static void check(bool v){need(v,"assertion failed");}
static void assertNear(double a,double b){need(std::abs(a-b)<1e-8,"numeric mismatch");}
static I base=1791000000000LL;
static J snap(double mid,I t,double spread=.02,const std::string& q="1000.00"){
 J x=demoSnapshot();x["records"]=J::array({x["records"][0]});auto& r=x["records"][0];r["market"]["close_time"]="2099-01-01T00:00:00Z";r["received_ms"]=t;
 r["orderbook"]={{"orderbook_fp",{{"yes_dollars",J::array({J::array({decimal(static_cast<I>(std::llround((mid-spread/2)*10000)),4),q})})},
 {"no_dollars",J::array({J::array({decimal(static_cast<I>(std::llround((1-mid-spread/2)*10000)),4),q})})}}}};return x;
}
static J warm(J& state,const MonitorConfig& c){J result;for(int i=0;i<8;++i)result=scoreMonitor(snap(.5,base+i*30000),state,c,base+i*30000);return result;}
static J next(J& state,const MonitorConfig& c,double mid,int n=8){return scoreMonitor(snap(mid,base+n*30000),state,c,base+n*30000)["markets"][0];}
int main(){MonitorConfig c=monitorConfig(J::object());
 test("median resists an extreme observation",[]{assertNear(median({.49,.50,.51,.50,.99}),.5);});
 test("even median averages middle pair",[]{assertNear(median({.1,.2,.3,.4}),.25);});
 test("strict UTC close parser",[]{check(utcMillis("2026-02-30T01:00:00Z")==0);check(utcMillis("2030-01-01T00:00:00.123Z")>0);check(utcMillis("garbage")==0);});
 test("first observations require warmup",[&]{auto s=emptyMonitorState();auto r=warm(s,c)["markets"][0];check(r["monitor_status"]=="warming_up"&&!r["candidate"].get<bool>());});
 test("flat history has finite zero score",[&]{auto s=emptyMonitorState();warm(s,c);auto r=next(s,c,.5);assertNear(num(r,"robust_z"),0);check(r["monitor_status"]=="normal");});
 test("single extreme move is unconfirmed and quarantined",[&]{auto s=emptyMonitorState();warm(s,c);auto r=next(s,c,.4);check(!r["candidate"].get<bool>()&&r["monitor_status"]=="unconfirmed_move");check(s["markets"]["DEMO-1"]["history"].size()==8);});
 test("second low quote identifies conditional YES gap",[&]{auto s=emptyMonitorState();warm(s,c);next(s,c,.4);auto r=next(s,c,.4,9);check(r["candidate"].get<bool>()&&r["best"]["side"]=="yes");assertNear(num(r,"reference"),.5);assertNear(num(r,"gap"),.1);check(num(r["best"],"lower_edge")<.1);});
 test("higher YES quotes compare NO purchase",[&]{auto s=emptyMonitorState();warm(s,c);next(s,c,.6);auto r=next(s,c,.6,9);check(r["candidate"].get<bool>()&&r["best"]["side"]=="no");assertNear(num(r,"gap"),-.1);});
 test("isolated spike never contaminates baseline",[&]{auto s=emptyMonitorState();warm(s,c);next(s,c,.4);auto r=next(s,c,.5,9);assertNear(num(r,"reference"),.5);check(r["monitor_status"]=="normal");check(s["markets"]["DEMO-1"]["quarantine"].empty());});
 test("persistent move retires old reference",[&]{auto s=emptyMonitorState();warm(s,c);J r;for(int n=8;n<13;++n)r=next(s,c,.4,n);check(r["monitor_status"]=="baseline_reset"&&!r["candidate"].get<bool>());check(s["markets"]["DEMO-1"]["history"].size()==5);assertNear(num(s["markets"]["DEMO-1"]["history"][0],"mid"),.4);});
 test("spread quality gate blocks false anomaly",[&]{auto s=emptyMonitorState();warm(s,c);auto r=scoreMonitor(snap(.4,base+240000,.15),s,c,base+240000)["markets"][0];check(r["monitor_status"]=="quality_blocked");});
 test("stale observation does not enter history",[&]{auto s=emptyMonitorState();warm(s,c);auto r=scoreMonitor(snap(.4,base+240000),s,c,base+400000)["markets"][0];check(!r["candidate"].get<bool>());check(s["markets"]["DEMO-1"]["history"].size()==8);});
 test("duplicate timestamp cannot confirm a move",[&]{auto s=emptyMonitorState();warm(s,c);next(s,c,.4);auto r=next(s,c,.4);check(r["monitor_status"]=="quality_blocked"&&!r["candidate"].get<bool>());});
 test("long gap resets history",[&]{auto s=emptyMonitorState();warm(s,c);auto r=next(s,c,.4,30);check(r["monitor_status"]=="warming_up");check(s["markets"]["DEMO-1"]["history"].size()==1);});
 test("closed market cannot be a candidate",[&]{auto s=emptyMonitorState();warm(s,c);auto x=snap(.4,base+240000);x["records"][0]["market"]["close_time"]="2000-01-01T00:00:00Z";check(scoreMonitor(x,s,c,base+240000)["markets"][0]["monitor_status"]=="quality_blocked");});
 test("one-sided book blocked",[&]{auto s=emptyMonitorState();auto x=snap(.4,base);x["records"][0]["orderbook"]["orderbook_fp"]["yes_dollars"]=J::array();check(scoreMonitor(x,s,c,base)["markets"][0]["monitor_status"]=="quality_blocked");});
 test("crossed book blocked",[&]{auto s=emptyMonitorState();auto x=snap(.5,base,-.02);check(scoreMonitor(x,s,c,base)["markets"][0]["monitor_status"]=="quality_blocked");});
 test("insufficient full-size depth suppresses candidate",[&]{auto s=emptyMonitorState();warm(s,c);for(int n=8;n<10;++n){auto r=scoreMonitor(snap(.4,base+n*30000,.02,"0.01"),s,c,base+n*30000)["markets"][0];check(!r["candidate"].get<bool>()&&r["best"].is_null());}});
 test("unknown fee never creates a net edge",[&]{auto s=emptyMonitorState();warm(s,c);for(int n=8;n<10;++n){auto x=snap(.4,base+n*30000);x["records"][0]["series"]["fee_type"]="flat";auto r=scoreMonitor(x,s,c,base+n*30000)["markets"][0];check(!r["candidate"].get<bool>()&&r["best"].is_null());}});
 test("rule changes reset temporal reference",[&]{auto s=emptyMonitorState();warm(s,c);auto x=snap(.4,base+240000);x["records"][0]["market"]["rules_primary"]="Changed rules";check(scoreMonitor(x,s,c,base+240000)["markets"][0]["monitor_status"]=="warming_up");});
 MonitorConfig model=c;model.forecasts={{"DEMO-1",{{"probability",.6},{"lower",.55},{"upper",.65},{"source","Test forecast"},{"as_of_ms",base-1000},{"valid_until_ms",base+1000000}}}};
 test("independent model confirms without historical warmup",[&]{auto s=emptyMonitorState();check(!next(s,model,.4,0)["candidate"].get<bool>());auto r=next(s,model,.4,1);check(r["candidate"].get<bool>()&&r["basis"]=="model");assertNear(num(r,"reference"),.6);});
 test("uncertainty pessimistic bound controls qualification",[&]{auto cfg=model;cfg.forecasts["DEMO-1"]["lower"]=.2;auto s=emptyMonitorState();next(s,cfg,.4,0);check(!next(s,cfg,.4,1)["candidate"].get<bool>());});
 test("expired model does not silently fall back",[&]{auto cfg=model;cfg.forecasts["DEMO-1"]["valid_until_ms"]=base-1;auto s=emptyMonitorState();warm(s,cfg);auto r=next(s,cfg,.4);check(r["monitor_status"]=="model_unavailable"&&r["edges"].empty());});
 test("future model timestamp rejected",[&]{auto cfg=model;cfg.forecasts["DEMO-1"]["as_of_ms"]=base+999999;auto s=emptyMonitorState();check(next(s,cfg,.4,0)["monitor_status"]=="model_unavailable");});
 test("changed forecast must confirm again",[&]{auto s=emptyMonitorState();next(s,model,.4,0);auto cfg=model;cfg.forecasts["DEMO-1"]["probability"]=.61;check(!next(s,cfg,.4,1)["candidate"].get<bool>());});
 test("buffer reduces per-contract edge exactly",[&]{auto a=analyzeRecord(snap(.4,base)["records"][0],c.settings,base);auto e=monitorEdges(a,.6,.55,.65,c);auto cfg=c;cfg.buffer+=.01;auto f=monitorEdges(a,.6,.55,.65,cfg);assertNear(num(e[0],"edge")-num(f[0],"edge"),.01);});
 test("historical estimate cannot see current point",[&]{auto s=emptyMonitorState();warm(s,c);auto r=next(s,c,.3);assertNear(num(r,"reference"),.5);});
 test("config changes reset baseline",[&]{auto s=emptyMonitorState();warm(s,c);auto cfg=c;cfg.window=30;check(next(s,cfg,.4)["monitor_status"]=="warming_up");});
 test("absent market breaks confirmation",[&]{auto s=emptyMonitorState();warm(s,c);next(s,c,.4);auto x=snap(.5,base+250000);x["records"]=J::array();scoreMonitor(x,s,c,base+250000);check(!next(s,c,.4,9)["candidate"].get<bool>());});
 test("minimum sample and invalid settings reject",[]{bool rejected=false;try{monitorConfig({{"min_samples",1}});}catch(...){rejected=true;}check(rejected);});
 test("real and synthetic histories cannot be mixed",[&]{auto s=emptyMonitorState();next(s,c,.5,0);auto x=snap(.5,base+30000);x["synthetic"]=false;bool rejected=false;try{scoreMonitor(x,s,c,base+30000);}catch(...){rejected=true;}check(rejected);});
 test("duplicate ticker rejects",[&]{auto s=emptyMonitorState();auto x=snap(.5,base);x["records"].push_back(x["records"][0]);bool rejected=false;try{scoreMonitor(x,s,c,base);}catch(...){rejected=true;}check(rejected);});
 std::cout<<passes<<" passed; "<<failures<<" failed\n";return failures?1:0;
}
