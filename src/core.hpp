#pragma once
#include "../third_party/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kl {
using J=nlohmann::json;
using I=std::int64_t;
inline void need(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
inline I nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline std::string utc(I ms){std::time_t t=static_cast<std::time_t>(ms/1000);std::tm b{};
#ifdef _WIN32
 gmtime_s(&b,&t);
#else
 gmtime_r(&t,&b);
#endif
 std::ostringstream o;o<<std::put_time(&b,"%Y-%m-%dT%H:%M:%SZ");return o.str();}
inline std::string str(const J& j,const char* k,const std::string& fallback=""){
 return j.contains(k)&&j[k].is_string()?j[k].get<std::string>():fallback;
}
inline double numeric(const J& v){double x; if(v.is_number())x=v.get<double>();else{
 need(v.is_string(),"Expected numeric field");auto s=v.get<std::string>();size_t n=0;x=std::stod(s,&n);need(n==s.size(),"Trailing numeric characters");}
 need(std::isfinite(x),"Non-finite number");return x;
}
inline double num(const J& j,const char* key,double fallback=0){return !j.contains(key)||j[key].is_null()?fallback:numeric(j[key]);}
                                                                                             
inline I fixed(const J& v,int decimals){
 need(v.is_string(),"Fixed-point API field must be a string");auto s=v.get<std::string>();
 need(!s.empty()&&s.size()<24,"Invalid fixed-point field");I whole=0,fraction=0;int digits=0;bool dot=false;
 for(char c:s){if(c=='.'&&!dot){dot=true;continue;}need(c>='0'&&c<='9',"Invalid fixed-point character");
  if(!dot){need(whole<=100000000,"Fixed-point value too large");whole=whole*10+c-'0';}
  else{need(digits<decimals||c=='0',"Unsupported fixed-point precision");if(digits<decimals)fraction=fraction*10+c-'0';++digits;}}
 need(s.front()!='.'&&s.back()!='.',"Invalid decimal format");
 I scale=1;for(int i=0;i<decimals;++i)scale*=10;for(int i=digits;i<decimals;++i)fraction*=10;
 return whole*scale+fraction;
}
inline std::string decimal(I value,int places){I scale=1;for(int i=0;i<places;++i)scale*=10;
 std::ostringstream s;s<<value/scale<<'.'<<std::setw(places)<<std::setfill('0')<<value%scale;return s.str();}
inline J nullable(double x){return std::isfinite(x)?J(x):J(nullptr);}
inline std::string hash(const std::string& text){std::uint64_t h=14695981039346656037ull;for(unsigned char c:text){h^=c;h*=1099511628211ull;}std::ostringstream s;s<<std::hex<<h;return s.str();}
struct Level{I p=0,q=0;};
inline std::vector<Level> levels(const J& array){
 need(array.is_array()||array.is_null(),"Orderbook side is not an array");std::map<I,I,std::greater<I>> merged;
 if(array.is_null())return {};
 need(array.size()<=10000,"Too many book levels");
 for(const auto& v:array){need(v.is_array()&&v.size()==2,"Invalid book level");I p=fixed(v[0],4),q=fixed(v[1],2);
  need(p>=0&&p<=10000&&q<=1000000000,"Book price or size out of range");if(q==0)continue;
  need(p>0&&p<10000,"Endpoint bid price requires manual review");merged[p]+=q;need(merged[p]<=1000000000,"Aggregated quantity too large");}
 std::vector<Level> result;for(auto [p,q]:merged)result.push_back({p,q});return result;
}
struct Book{std::vector<Level> yes,no;bool crossed=false;};
inline Book book(const J& j){need(j.contains("orderbook_fp")&&j["orderbook_fp"].is_object(),"Missing fixed-point orderbook (legacy schema unsupported)");
 const auto& b=j["orderbook_fp"];need(b.contains("yes_dollars")&&b.contains("no_dollars"),"Incomplete orderbook sides");
 Book result{levels(b["yes_dollars"]),levels(b["no_dollars"]),false};
 result.crossed=!result.yes.empty()&&!result.no.empty()&&result.yes[0].p+result.no[0].p>10000;return result;
}
inline std::vector<Level> asks(const Book& b,bool yes){auto a=yes?b.no:b.yes;for(auto& l:a)l.p=10000-l.p;return a;}
inline J levelJson(const std::vector<Level>& levels){J r=J::array();for(auto l:levels)r.push_back({{"price",l.p/10000.0},{"quantity",l.q/100.0}});return r;}
struct Fee{bool known=false;double multiplier=0;std::string type,source;};
inline Fee feePolicy(const J& r){Fee f;const J s=r.value("series",J::object()),e=r.value("event",J::object());
                                                                                
 if(!e.is_object()||!e.contains("event_ticker")){f.source="event fee overrides unavailable";return f;}
 f.type=str(e,"fee_type_override");if(f.type.empty())f.type=str(s,"fee_type");
 bool eventMultiplier=e.contains("fee_multiplier_override")&&!e["fee_multiplier_override"].is_null();
 if(eventMultiplier)f.multiplier=numeric(e["fee_multiplier_override"]);
 else if(s.contains("fee_multiplier")&&!s["fee_multiplier"].is_null())f.multiplier=numeric(s["fee_multiplier"]);
 else{f.source="fee multiplier missing";return f;}
 need(f.multiplier>=0&&f.multiplier<=100,"Fee multiplier out of range");
 f.known=f.type=="quadratic"||f.type=="quadratic_with_maker_fees";
 f.source=eventMultiplier||!str(e,"fee_type_override").empty()?"event override + series metadata":"series metadata";
 if(!f.known)f.source="unsupported fee type: "+f.type;
 return f;
}
struct FeeAccumulator{I rounding=0;I grid=100;                                 
 I charge(I price,I quantity,double multiplier){
  need(price>=0&&price<=10000&&quantity>=0,"Invalid fee arguments");
  long double p=price/10000.0L,q=quantity/100.0L;
  long double raw=0.07L*multiplier*q*p*(1-p)*1000000;
  I trade=static_cast<I>(std::ceil(raw-1e-9L));
  I cost=price*quantity;                      
  I total=cost+trade;I rounded=((total+grid-1)/grid)*grid;
  I extra=rounded-total;rounding+=extra;
  I rebate=std::min((rounding/grid)*grid,((trade+extra)/grid)*grid);rounding-=rebate;
  return trade+extra-rebate;
 }
};
struct Sweep{I requested=0,filled=0,notional=0,fees=0,worst=0;int touched=0;bool feesKnown=false;};
inline Sweep sweep(const std::vector<Level>& offers,I requested,const Fee& fee,I grid=100){
 need(requested>0&&requested<=1000000000,"Requested size must be between 0 and 10 million contracts");
 need(grid==100||grid==10000,"Invalid member balance precision");
 Sweep s;s.requested=requested;s.feesKnown=fee.known;FeeAccumulator acc{0,grid};
 for(auto l:offers){I take=std::min(l.q,requested-s.filled);if(take<=0)break;
  s.filled+=take;s.notional+=l.p*take;s.worst=l.p;++s.touched;if(fee.known)s.fees+=acc.charge(l.p,take,fee.multiplier);}
 return s;
}
inline J sweepJson(const Sweep& s){double q=s.filled/100.0;
 return {{"requested",s.requested/100.0},{"filled",q},{"complete",s.filled==s.requested},
 {"notional",s.notional/1000000.0},{"fee",s.feesKnown?J(s.fees/1000000.0):J(nullptr)},
 {"cost",s.feesKnown?J((s.notional+s.fees)/1000000.0):J(nullptr)},
 {"vwap",q>0?J(s.notional/1000000.0/q):J(nullptr)},
 {"break_even",q>0&&s.feesKnown?J((s.notional+s.fees)/1000000.0/q):J(nullptr)},
 {"worst_price",q>0?J(s.worst/10000.0):J(nullptr)},{"levels",s.touched}};
}
struct Settings{I quantity=10000,grid=100;double maxSkew=10,staleAfter=60;J beliefs=J::object(),relationships=J::array();};
inline J scenario(const Sweep& s,double yesProbability,bool yes,double uncertainty){
 need(yesProbability>=0&&yesProbability<=1&&uncertainty>=0&&uncertainty<=1,"Probability/uncertainty outside [0,1]");
 if(!s.feesKnown||s.filled!=s.requested||s.filled==0)return nullptr;
 double q=s.filled/100.0,c=(s.notional+s.fees)/1000000.0;
 double p=yes?yesProbability:1-yesProbability;
 double low=std::max(0.0,p-uncertainty),high=std::min(1.0,p+uncertainty);
 return {{"probability",p},{"probability_low",low},{"probability_high",high},{"ev",q*p-c},
 {"ev_low",q*low-c},{"ev_high",q*high-c},{"max_loss",c},{"profit_if_win",q-c},
 {"roi_expected",c>0?J((q*p-c)/c):J(nullptr)}};
}
inline J analyzeRecord(const J& r,const Settings& settings,I referenceMs){
 const auto& m=r.at("market");std::string ticker=str(m,"ticker");need(!ticker.empty(),"Market ticker missing");
 J a={{"ticker",ticker},{"title",str(m,"title")},{"subtitle",str(m,"yes_sub_title",str(m,"subtitle"))},
 {"event_ticker",str(m,"event_ticker")},{"series_ticker",str(r.value("event",J::object()),"series_ticker")},
 {"category",str(r.value("series",J::object()),"category","Unclassified")},{"status",str(m,"status")},
 {"close_time",str(m,"close_time")},{"rules_primary",str(m,"rules_primary")},{"rules_secondary",str(m,"rules_secondary")},
 {"volume",num(m,"volume_fp")},{"volume_24h",num(m,"volume_24h_fp")},{"open_interest",num(m,"open_interest_fp")},
 {"last_price",m.contains("last_price_dollars")?J(num(m,"last_price_dollars")):J(nullptr)},
 {"captured_ms",r.value("received_ms",I(0))},{"warnings",J::array()},{"valid",false}};
 for(const auto& error:r.value("errors",J::array()))a["warnings"].push_back(error);
 if(str(m,"market_type")!="binary"||num(m,"notional_value_dollars",-1)!=1){a["warnings"].push_back("Only explicit $1 binary markets are supported");return a;}
 if(m.contains("mve_collection_ticker")&&!str(m,"mve_collection_ticker").empty()){a["warnings"].push_back("Multivariate products are outside the model");return a;}
 try{
 Book b=book(r.at("orderbook"));Fee f=feePolicy(r);
 a["fee"]={{"known",f.known},{"multiplier",f.multiplier},{"type",f.type},{"source",f.source}};
 a["yes_bids"]=levelJson(b.yes);a["no_bids"]=levelJson(b.no);a["yes_asks"]=levelJson(asks(b,true));a["no_asks"]=levelJson(asks(b,false));
 a["yes_bid"]=b.yes.empty()?J(nullptr):J(b.yes[0].p/10000.0);
 a["yes_ask"]=b.no.empty()?J(nullptr):J((10000-b.no[0].p)/10000.0);
 a["no_bid"]=b.no.empty()?J(nullptr):J(b.no[0].p/10000.0);
 a["no_ask"]=b.yes.empty()?J(nullptr):J((10000-b.yes[0].p)/10000.0);
 a["mid"]=nullptr;a["spread"]=nullptr;a["imbalance"]=nullptr;
 if(!b.yes.empty()&&!b.no.empty()){
  a["mid"]=(b.yes[0].p+10000-b.no[0].p)/20000.0;
  a["spread"]=(10000-b.no[0].p-b.yes[0].p)/10000.0;
  I y=0,n=0;for(auto l:b.yes)if(l.p>=b.yes[0].p-200)y+=l.q;
  for(auto l:b.no)if(l.p>=b.no[0].p-200)n+=l.q;
  if(y+n>0)a["imbalance"]=(y-n)/static_cast<double>(y+n);
 }
 I captured=r.value("received_ms",I(0));a["age_seconds"]=(referenceMs-captured)/1000.0;
 bool stale=captured<=0||referenceMs-captured>static_cast<I>(settings.staleAfter*1000)||captured>referenceMs+1000;
 a["stale"]=stale;
 if(stale)a["warnings"].push_back("Snapshot timestamp is stale, missing, or in the future");
 if(!f.known)a["warnings"].push_back("Fee-adjusted value unavailable: "+f.source);
 if(b.yes.empty()||b.no.empty())a["warnings"].push_back("One-sided or empty book; midpoint may be unavailable");
 if(b.crossed){a["warnings"].push_back("Crossed binary book; execution estimates suppressed");return a;}
 if(str(m,"status")!="active"&&str(m,"status")!="open"){a["warnings"].push_back("Market not active; execution estimates suppressed");return a;}
 if(m.contains("fee_waiver_expiration_time")&&!str(m,"fee_waiver_expiration_time").empty())
  a["warnings"].push_back("Potential fee waiver present; estimate uses metadata without waiver credit");
 a["valid"]=true;a["executable_snapshot"]=!stale&&f.known;
 a["sizes"]=J::array();std::set<I> sizes={100,1000,10000,100000,settings.quantity};
 for(I size:sizes){auto sy=sweep(asks(b,true),size,f,settings.grid),sn=sweep(asks(b,false),size,f,settings.grid);
  a["sizes"].push_back({{"quantity",size/100.0},{"yes",sweepJson(sy)},{"no",sweepJson(sn)}});}
 auto sy=sweep(asks(b,true),settings.quantity,f,settings.grid),sn=sweep(asks(b,false),settings.quantity,f,settings.grid);
 a["selected"]={{"yes",sweepJson(sy)},{"no",sweepJson(sn)}};
 a["belief"]=nullptr;a["scenario_yes"]=nullptr;a["scenario_no"]=nullptr;
 if(settings.beliefs.contains(ticker)){
  auto belief=settings.beliefs[ticker];double p=numeric(belief.at("probability")),u=num(belief,"uncertainty",0.05);
  need(!str(belief,"source").empty(),"Belief requires a source/justification");
  a["belief"]=belief;a["scenario_yes"]=scenario(sy,p,true,u);a["scenario_no"]=scenario(sn,p,false,u);
 }
 }catch(const std::exception& e){a["valid"]=false;a["warnings"].push_back(e.what());}
 return a;
}
inline J structural(const J& rows,const J& records,const Settings& s){
 J checks=J::array();std::map<std::string,J> byTicker,events;std::map<std::string,std::vector<std::string>> grouped;
 for(const auto& a:rows){byTicker[a["ticker"].get<std::string>()]=a;grouped[str(a,"event_ticker")].push_back(str(a,"ticker"));}
 for(const auto& r:records)if(r.contains("event"))events[str(r["event"],"event_ticker")]=r["event"];
 auto basket=[&](const std::string& label,const std::vector<std::pair<std::string,bool>>& legs,double payout,const std::string& assumption){
  J c={{"label",label},{"assumption",assumption},{"quantity",s.quantity/100.0},{"payout_floor",payout*s.quantity/100.0},
       {"legs",J::array()},{"cost",nullptr},{"net_floor",nullptr},{"candidate",false},{"issues",J::array()}};
  double cost=0;I early=std::numeric_limits<I>::max(),late=0;bool ok=true;
  for(const auto& [ticker,yes]:legs){c["legs"].push_back({{"ticker",ticker},{"side",yes?"YES":"NO"}});
   if(!byTicker.count(ticker)){ok=false;c["issues"].push_back("Market not in snapshot: "+ticker);continue;}
   const auto& a=byTicker[ticker];
   if(!a.value("valid",false)||!a.value("executable_snapshot",false)){ok=false;c["issues"].push_back("Invalid, stale or unknown-fee leg: "+ticker);continue;}
   const auto& fill=a["selected"][yes?"yes":"no"];
   if(!fill.value("complete",false)||fill["cost"].is_null()){ok=false;c["issues"].push_back("Insufficient depth: "+ticker);continue;}
   cost+=fill["cost"].get<double>();I t=a["captured_ms"].get<I>();early=std::min(early,t);late=std::max(late,t);
  }
  if(late>0&&early!=std::numeric_limits<I>::max()&&(late-early)/1000.0>s.maxSkew){ok=false;c["issues"].push_back("Leg snapshots exceed allowed time skew");}
  if(ok){c["cost"]=cost;c["net_floor"]=payout*s.quantity/100.0-cost;c["candidate"]=payout*s.quantity/100.0-cost>0.000001;}
  c["issues"].push_back("Theoretical binary-settlement bound only; review rules, exceptional settlement and non-atomic leg risk");checks.push_back(c);
 };
 for(const auto& [event,tickers]:grouped){if(tickers.size()<2||!events.count(event))continue;
  if(events[event].value("mutually_exclusive",false)){
   std::vector<std::pair<std::string,bool>> legs;for(const auto& t:tickers)legs.push_back({t,false});
   basket("Mutually exclusive NO basket: "+event,legs,static_cast<double>(tickers.size()-1),
          "At most one of the included markets settles YES; remaining included markets settle NO at $1. Exhaustiveness is NOT assumed.");
  }
 }
 for(const auto& rel:s.relationships){
  if(!rel.value("reviewed",false))continue;
  need(str(rel,"type")=="implies","Only reviewed implication relationships are supported");
  std::string a=str(rel,"antecedent"),b=str(rel,"consequent");need(!a.empty()&&!b.empty()&&a!=b&&!str(rel,"rationale").empty(),"Invalid reviewed relationship");
  basket("Reviewed implication: "+a+" -> "+b,{{a,false},{b,true}},1,str(rel,"rationale"));
 }
 return checks;
}
inline J analyze(const J& snapshot,const Settings& s){
 need(snapshot.value("schema","")=="kalshilens.snapshot.v1","Unsupported snapshot schema");
 need(snapshot.contains("records")&&snapshot["records"].is_array()&&snapshot["records"].size()<=1000,"Invalid snapshot records");
 J rows=J::array();I asof=nowMs();std::set<std::string> seen;
 for(const auto& r:snapshot["records"]){auto a=analyzeRecord(r,s,asof);need(seen.insert(str(a,"ticker")).second,"Duplicate market ticker in snapshot");rows.push_back(a);}
 J checks=structural(rows,snapshot["records"],s);
 return {{"schema","kalshilens.analysis.v1"},{"version","1.0.0"},{"generated_at",utc(asof)},
 {"snapshot_started_at",snapshot.value("started_at","")},{"snapshot_completed_at",snapshot.value("completed_at","")},
 {"synthetic",snapshot.value("synthetic",false)},{"source",snapshot.value("source","")},
 {"coverage",snapshot.value("coverage",J::object())},{"snapshot_fingerprint_fnv1a64",hash(snapshot.dump())},
 {"quantity",s.quantity/100.0},{"balance_precision",s.grid/1000000.0},{"max_skew_seconds",s.maxSkew},
 {"stale_after_seconds",s.staleAfter},{"markets",rows},{"structural_checks",checks},
 {"limitations",J::array({"Prices are not objective probabilities.","Displayed depth can cancel before any hypothetical execution.",
 "Each price level is modeled as one fill; actual fill fragmentation and fee rebates can differ.",
 "Series/event fee metadata is a snapshot; account-specific discounts and exceptional schedules require verification.",
 "Cross-market observations are sequential, not simultaneous. No orders are sent."})}};
}
inline J calibration(const J& input){
 need(input.is_array()&&!input.empty(),"Forecast outcomes must be a nonempty JSON array");
 double brier=0,loss=0;std::set<std::string> ids;J bins=J::array();
 for(int i=0;i<10;++i)bins.push_back({{"low",i/10.0},{"high",(i+1)/10.0},{"count",0},{"wins",0},{"sum_p",0.0}});
 for(const auto& v:input){std::string id=str(v,"id");need(!id.empty()&&ids.insert(id).second,"Forecast IDs must be nonempty and unique");
  double p=numeric(v.at("probability")),y=numeric(v.at("outcome"));need(p>=0&&p<=1&&(y==0||y==1),"Invalid forecast/outcome");
  brier+=(p-y)*(p-y);double clipped=std::clamp(p,1e-15,1-1e-15);loss-=y*std::log(clipped)+(1-y)*std::log(1-clipped);
  int i=std::min(9,static_cast<int>(p*10));auto& b=bins[i];b["count"]=b["count"].get<int>()+1;b["wins"]=b["wins"].get<int>()+static_cast<int>(y);b["sum_p"]=b["sum_p"].get<double>()+p;
 }
 for(auto& b:bins){double n=b["count"].get<int>();if(n==0){b["observed"]=nullptr;b["predicted"]=nullptr;continue;}
  double p=b["wins"].get<int>()/n,z=1.96,den=1+z*z/n,center=(p+z*z/(2*n))/den;
  double half=z*std::sqrt(p*(1-p)/n+z*z/(4*n*n))/den;
  b["observed"]=p;b["predicted"]=b["sum_p"].get<double>()/n;b["wilson_low"]=center-half;b["wilson_high"]=center+half;
 }
 return {{"count",input.size()},{"brier",brier/static_cast<double>(input.size())},{"log_loss",loss/static_cast<double>(input.size())},
 {"bins",bins},{"note","Record predictions before outcomes. Wilson intervals assume independent Bernoulli outcomes; related markets violate that assumption. Log loss clips exact 0/1 to 1e-15."}};
}
}                
