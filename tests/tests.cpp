#include "../src/client.hpp"
#include <functional>
using namespace kl;
static void assertNear(double a,double b){need(std::abs(a-b)<=1e-8*std::max(1.0,std::abs(b)),"Expected "+std::to_string(b)+", got "+std::to_string(a));}
static void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}need(caught,"Expected rejection");}
static J fixture(){return demoSnapshot()["records"][0];}
static Fee standard(){return {true,1,"quadratic","test"};}
int main(){int passed=0,failed=0;auto test=[&](const char* name,const std::function<void()>& f){try{f();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
 test("fixed point preserves subpenny price",[]{need(fixed(J("0.1234"),4)==1234,"Precision loss");});
 test("fixed point preserves fractional contracts",[]{need(fixed(J("13.27"),2)==1327,"Fraction lost");});
 test("excess precision never silently rounds",[]{rejects([]{fixed(J("0.12345"),4);});need(fixed(J("0.123400"),4)==1234,"Trailing zeros");});
 test("malformed and negative fixed point reject",[]{for(auto s:{"-0.1","NaN","1e2",".5","5.","","1.2.3"})rejects([&]{fixed(J(s),4);});});
 test("book sorting and duplicate aggregation",[]{auto l=levels(J::array({J::array({"0.20","1.00"}),J::array({"0.40","2.00"}),J::array({"0.40","3.00"})}));need(l.size()==2&&l[0].p==4000&&l[0].q==500,"Aggregation failed");});
 test("YES ask derives from NO bid",[]{Book b{{{3500,1000}},{{6200,2000}},false};auto a=asks(b,true);need(a[0].p==3800&&a[0].q==2000,"Complement failure");});
 test("NO ask derives from YES bid",[]{Book b{{{3500,1000}},{{6200,2000}},false};need(asks(b,false)[0].p==6500,"Complement failure");});
 test("zero quantity contributes no liquidity",[]{need(levels(J::array({J::array({"0.5","0.00"})})).empty(),"Zero size retained");});
 test("legacy or missing orderbook is rejected",[]{rejects([]{book(J{{"orderbook",{{"yes",J::array()}}}});});});
 test("out-of-range price rejected",[]{rejects([]{levels(J::array({J::array({"1.01","10.00"})}));});});
 test("direct-member fee at 50 cents",[]{FeeAccumulator a;need(a.charge(5000,100,1)==17500,"Expected $0.0175");});
 test("FCM fee aligns complete balance change",[]{FeeAccumulator a{0,10000};need(a.charge(5000,100,1)==20000,"Expected $0.02");});
 test("100 contracts at 50 cents costs 1.75 fee",[]{FeeAccumulator a;need(a.charge(5000,10000,1)==1750000,"Wrong fee");});
 test("fee multiplier applies before rounding",[]{FeeAccumulator a;need(a.charge(5000,10000,2)==3500000,"Multiplier ignored");});
 test("zero multiplier no invented fee at aligned price",[]{FeeAccumulator a;need(a.charge(5000,100,0)==0,"Invented fee");});
 test("fee accumulator rebates over-rounding",[]{FeeAccumulator a{0,10000};I fee=0;for(int i=0;i<3;++i)fee+=a.charge(5000,100,1);need(fee==60000&&a.rounding==7500,"Rounding/rebate accounting");});
 test("fractional notional is exact microdollars",[]{auto x=sweep({{1234,127}},127,standard());need(x.notional==156718,"Notional not exact");});
 test("depth walk computes multiple levels",[]{auto x=sweep({{4000,5000},{5000,10000}},10000,standard());need(x.filled==10000&&x.notional==45000000&&x.worst==5000&&x.touched==2,"Depth walk wrong");});
 test("insufficient depth remains partial",[]{auto x=sweep({{4000,500}},1000,standard());need(x.filled==500&&!sweepJson(x)["complete"].get<bool>(),"Fabricated liquidity");});
 test("unknown fees remain null",[]{Fee f;auto x=sweep({{4000,500}},500,f);need(sweepJson(x)["cost"].is_null(),"Invented unknown fee");});
 test("event fee overrides series",[]{auto r=fixture();r["event"]["fee_multiplier_override"]=2;auto f=feePolicy(r);need(f.known&&f.multiplier==2,"Override ignored");});
 test("flat fee type is not guessed",[]{auto r=fixture();r["series"]["fee_type"]="flat";need(!feePolicy(r).known,"Guessed flat fee");});
 test("missing event metadata suppresses known fee claim",[]{auto r=fixture();r["event"]=J::object();need(!feePolicy(r).known,"Missing override metadata ignored");});
 test("negative fee multiplier rejects",[]{auto r=fixture();r["series"]["fee_multiplier"]=-1;rejects([&]{feePolicy(r);});});
 test("crossed book blocks execution",[]{auto r=demoSnapshot()["records"][9];auto a=analyzeRecord(r,Settings{},nowMs());need(!a["valid"].get<bool>()&&!a.contains("selected"),"Crossed book treated as opportunity");});
 test("one-sided book has no invented midpoint",[]{auto r=demoSnapshot()["records"][7];auto a=analyzeRecord(r,Settings{},nowMs());need(a["mid"].is_null()&&a["no_ask"].is_null(),"Invented quote");});
 test("stale snapshot cannot support structural candidate",[]{auto r=fixture();r["received_ms"]=nowMs()-120000;auto a=analyzeRecord(r,Settings{},nowMs());need(a["stale"].get<bool>()&&!a["executable_snapshot"].get<bool>(),"Stale data used");});
 test("future timestamp is flagged",[]{auto r=fixture();r["received_ms"]=nowMs()+100000;need(analyzeRecord(r,Settings{},nowMs())["stale"].get<bool>(),"Clock error ignored");});
 test("non-dollar notional rejected",[]{auto r=fixture();r["market"]["notional_value_dollars"]="10.0000";need(!analyzeRecord(r,Settings{},nowMs())["valid"].get<bool>(),"Wrong contract payout");});
 test("inactive markets cannot produce execution estimates",[]{auto r=fixture();r["market"]["status"]="settled";need(!analyzeRecord(r,Settings{},nowMs())["valid"].get<bool>(),"Inactive allowed");});
 test("EV uses user probability not midpoint",[]{auto x=sweep({{4000,10000}},10000,standard());auto e=scenario(x,.6,true,.1);assertNear(e["ev"],60-40-1.68);assertNear(e["ev_low"],50-40-1.68);});
 test("NO scenario complements YES probability",[]{auto x=sweep({{4000,10000}},10000,standard());auto e=scenario(x,.7,false,.1);assertNear(e["probability"],.3);assertNear(e["ev_low"],20-40-1.68);});
 test("partial fill cannot masquerade as full-size EV",[]{auto x=sweep({{4000,100}},10000,standard());need(scenario(x,.6,true,.1).is_null(),"Partial EV accepted");});
 test("invalid probabilities reject",[]{auto x=sweep({{4000,10000}},10000,standard());rejects([&]{scenario(x,1.1,true,.1);});});
 test("duplicate snapshot ticker rejects",[]{auto d=demoSnapshot();d["records"].push_back(d["records"][0]);rejects([&]{analyze(d,Settings{});});});
 test("mutual exclusion does not assume exhaustive YES basket",[]{auto a=analyze(demoSnapshot(),Settings{});need(a["structural_checks"].size()==1,"Unexpected checks");auto c=a["structural_checks"][0];assertNear(c["payout_floor"],200);for(auto l:c["legs"])need(l["side"]=="NO","Invented exhaustive YES basket");});
 test("unreviewed implication never evaluated",[]{Settings s;s.relationships=J::array({{{"type","implies"},{"antecedent","DEMO-6"},{"consequent","DEMO-5"},{"reviewed",false}}});auto a=analyze(demoSnapshot(),s);need(a["structural_checks"].size()==1,"Unreviewed inference");});
 test("reviewed implication requires rationale",[]{Settings s;s.relationships=J::array({{{"type","implies"},{"antecedent","DEMO-6"},{"consequent","DEMO-5"},{"reviewed",true}}});rejects([&]{analyze(demoSnapshot(),s);});});
 test("cross-leg skew suppresses bound",[]{auto d=demoSnapshot();d["records"][1]["received_ms"]=nowMs()-20000;auto a=analyze(d,Settings{});need(a["structural_checks"][0]["cost"].is_null(),"Unsynchronized basket");});
 test("unsupported snapshot schema rejects",[]{auto d=demoSnapshot();d["schema"]="v0";rejects([&]{analyze(d,Settings{});});});
 test("perfect forecasts have zero Brier",[]{auto r=calibration(J::array({{{"id","a"},{"probability",1},{"outcome",1}},{{"id","b"},{"probability",0},{"outcome",0}}}));assertNear(r["brier"],0);});
 test("50 percent forecasts have Brier 0.25",[]{auto r=calibration(J::array({{{"id","a"},{"probability",.5},{"outcome",1}},{{"id","b"},{"probability",.5},{"outcome",0}}}));assertNear(r["brier"],.25);assertNear(r["log_loss"],std::log(2));});
 test("duplicate forecast IDs rejected",[]{J a={{"id","x"},{"probability",.5},{"outcome",1}};rejects([&]{calibration(J::array({a,a}));});});
 test("invalid settlement outcome rejected",[]{rejects([]{calibration(J::array({{{"id","a"},{"probability",.5},{"outcome",.5}}}));});});
 test("pagination coverage and metadata caching",[]{auto r=fixture();int page=0,events=0,series=0;CollectOptions opt;opt.maxMarkets=2;
  auto fetch=[&](const std::string& path)->J{if(path.rfind("/markets?",0)==0){++page;J m=r["market"];m["ticker"]=page==1?"A":"B";return {{"markets",J::array({m})},{"cursor",page==1?"next":""}};}
   if(path.rfind("/events/",0)==0){++events;return {{"event",r["event"]}};}if(path.rfind("/series/",0)==0){++series;return {{"series",r["series"]}};}return r["orderbook"];};
  auto d=collect(opt,fetch);need(d["records"].size()==2&&page==2&&events==1&&series==1,"Pagination/cache error");});
 test("repeating pagination cursor rejects",[]{CollectOptions o;o.pages=3;auto r=fixture();rejects([&]{collect(o,[&](const std::string&)->J{return {{"markets",J::array({r["market"]})},{"cursor","repeat"}};});});});
 test("API errors retain an explicitly invalid market",[]{CollectOptions o;o.ticker="DEMO-1";auto r=fixture();auto d=collect(o,[&](const std::string& p)->J{if(p=="/markets/DEMO-1")return {{"market",r["market"]}};throw std::runtime_error("offline");});auto a=analyze(d,Settings{});need(!a["markets"][0]["valid"].get<bool>()&&a["markets"][0]["warnings"].size()>=2,"Failure hidden");});
 test("URL encoding prevents query injection",[]{need(encode("A&B?x")=="A%26B%3Fx","Unsafe encoding");});
 test("snapshot comparison measures quote change",[]{auto d=demoSnapshot(),e=d;e["records"][0]["orderbook"]["orderbook_fp"]["no_dollars"][0][0]="0.5700";auto c=compare(analyze(d,Settings{}),analyze(e,Settings{}));assertNear(c["rows"][0]["change"],.005);});
 std::cout<<passed<<" passed; "<<failed<<" failed\n";return failed?1:0;
}
