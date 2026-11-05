#pragma once
#include "client.hpp"

namespace kl {
                                                                                         
inline double median(std::vector<double> v) {
    need(!v.empty(), "Median needs observations");
    std::sort(v.begin(), v.end());
    size_t n=v.size();return n%2?v[n/2]:(v[n/2-1]+v[n/2])/2;
}
inline I utcMillis(const std::string& s) {
    if(s.size()<20 || s.back()!='Z')return 0;
    if(s.size()>20 && (s[19]!='.' || !std::all_of(s.begin()+20,s.end()-1,[](char c){return c>='0'&&c<='9';})))return 0;
    std::tm tm{};std::istringstream input(s.substr(0,19));input>>std::get_time(&tm,"%Y-%m-%dT%H:%M:%S");
    if(input.fail())return 0;
#ifdef _WIN32
    auto value=_mkgmtime(&tm);
#else
    auto value=timegm(&tm);
#endif
    if(value<0)return 0;
    I t=static_cast<I>(value)*1000;
    return utc(t).substr(0,19)==s.substr(0,19)?t:0;
}
struct MonitorConfig {
    int window=60,minSamples=8,confirm=2,rebase=5;
    double maxAge=60,maxGap=180,lookback=7200,minSpan=60,maxSpread=.08,zThreshold=3.5,minMove=.02;
    double sigmaFloor=.005,buffer=.005,minEdge=.01;
    Settings settings;J forecasts=J::object();
};
inline MonitorConfig monitorConfig(const J& j) {
    need(j.is_object(),"Monitor configuration must be an object");MonitorConfig c;c.settings.quantity=1000;
    const std::set<std::string> keys={"quantity","member","window","min_samples","confirmations","rebase_after","max_age_seconds","max_gap_seconds","lookback_seconds","min_span_seconds","max_spread","z_threshold","min_move","sigma_floor","execution_buffer","min_edge","forecasts"};
    for(auto i=j.begin();i!=j.end();++i)need(keys.count(i.key())>0,"Unknown monitor setting: "+i.key());
    auto integerSetting=[&](const char* k,int old,int low,int high){double x=num(j,k,old);need(x==std::floor(x)&&x>=low&&x<=high,std::string("Invalid ")+k);return static_cast<int>(x);};
    c.window=integerSetting("window",60,8,600);c.minSamples=integerSetting("min_samples",8,8,c.window);
    c.confirm=integerSetting("confirmations",2,2,10);c.rebase=integerSetting("rebase_after",5,c.confirm+1,30);
    c.maxAge=num(j,"max_age_seconds",60);c.maxGap=num(j,"max_gap_seconds",180);c.lookback=num(j,"lookback_seconds",7200);c.minSpan=num(j,"min_span_seconds",60);
    c.maxSpread=num(j,"max_spread",.08);c.zThreshold=num(j,"z_threshold",3.5);c.minMove=num(j,"min_move",.02);c.sigmaFloor=num(j,"sigma_floor",.005);
    c.buffer=num(j,"execution_buffer",.005);c.minEdge=num(j,"min_edge",.01);
    need(c.maxAge>=1&&c.maxAge<=300&&c.maxGap>=c.maxAge&&c.maxGap<=3600&&c.lookback>=c.maxGap&&c.lookback<=86400&&c.minSpan>=1&&c.minSpan<=c.lookback,"Invalid time thresholds");
    need(c.maxSpread>0&&c.maxSpread<=.5&&c.zThreshold>=2&&c.zThreshold<=20&&c.minMove>0&&c.minMove<=.5&&c.sigmaFloor>=.0001&&c.sigmaFloor<=.1&&c.buffer>=0&&c.buffer<=.1&&c.minEdge>=.001&&c.minEdge<=.5,"Invalid price thresholds");
    double q=num(j,"quantity",10);need(q>0&&q<=100000&&std::abs(q*100-std::round(q*100))<1e-6,"Invalid quantity");c.settings.quantity=static_cast<I>(std::llround(q*100));
    std::string member=str(j,"member","direct");need(member=="direct"||member=="fcm","Invalid member");c.settings.grid=member=="direct"?100:10000;c.settings.staleAfter=c.maxAge;
    c.forecasts=j.value("forecasts",J::object());need(c.forecasts.is_object(),"Forecasts must map tickers to estimates");return c;
}
inline J emptyMonitorState(){return {{"schema","kalshilens.monitor-state.v1"},{"markets",J::object()}};}
inline J freshTrack(){return {{"history",J::array()},{"chart",J::array()},{"quarantine",J::array()},{"last",0},{"direction",0},{"streak",0},{"model_streak",0},{"model_side",""}};}
inline J monitorEdges(const J& r,double p,double low,double high,const MonitorConfig& c) {
    J result=J::array();
    for(const std::string side:{"yes","no"}) {
        if(!r.contains("selected"))continue;
        const auto& f=r["selected"][side];
        if(!f.value("complete",false)||f["cost"].is_null()||num(f,"filled")<=0)continue;
        double q=num(f,"filled"),cost=num(f,"cost"),value=side=="yes"?p:1-p;
        double lower=side=="yes"?low:1-high,upper=side=="yes"?high:1-low;
        double unit=cost/q+c.buffer;
        result.push_back({{"side",side},{"quantity",q},{"vwap",f["vwap"]},{"fee",f["fee"]},{"cost",cost},
            {"extra_buffer",c.buffer*q},{"break_even",unit},{"reference_probability",value},{"lower_probability",lower},{"upper_probability",upper},
            {"edge",value-unit},{"lower_edge",lower-unit},{"upper_edge",upper-unit},{"expected_profit",q*(value-unit)},
            {"conservative_profit",q*(lower-unit)},{"roi",unit>0?(value-unit)/unit:0}});
    }
    std::sort(result.begin(),result.end(),[](const J& a,const J& b){return num(a,"lower_edge")>num(b,"lower_edge");});return result;
}
inline J scoreMonitor(const J& snapshot,J& state,const MonitorConfig& c,I now=nowMs()) {
    need(snapshot.value("schema","")=="kalshilens.snapshot.v1","Unsupported snapshot");
    need(state.value("schema","")=="kalshilens.monitor-state.v1"&&state["markets"].is_object(),"Unsupported monitor state");
    need(snapshot.at("records").is_array()&&snapshot["records"].size()<=1000,"Too many records");
    bool synthetic=snapshot.value("synthetic",false);
    if(state.contains("synthetic"))need(state["synthetic"]==synthetic,"Synthetic and real history cannot be mixed");
    state["synthetic"]=synthetic;
    const std::string configSignature=hash(J{{"window",c.window},{"minSamples",c.minSamples},{"confirm",c.confirm},{"rebase",c.rebase},{"maxGap",c.maxGap},{"lookback",c.lookback},{"minSpan",c.minSpan},{"maxSpread",c.maxSpread},{"z",c.zThreshold},{"minMove",c.minMove},{"floor",c.sigmaFloor},{"buffer",c.buffer},{"minEdge",c.minEdge},{"quantity",c.settings.quantity},{"grid",c.settings.grid},{"maxAge",c.maxAge}}.dump());
    if(state.value("config_signature",configSignature)!=configSignature)state["markets"]=J::object();
    state["config_signature"]=configSignature;
    auto& tracks=state["markets"];
    for(auto it=tracks.begin();it!=tracks.end();) {
        if(now-it.value().value("last",I(0))>86400000)it=tracks.erase(it);else ++it;
    }
    need(tracks.size()<=1000,"State market limit exceeded");J rows=J::array();std::set<std::string> seen;
    for(const auto& record:snapshot["records"]) {
        J r=analyzeRecord(record,c.settings,now);std::string ticker=str(r,"ticker");need(seen.insert(ticker).second,"Duplicate ticker");
        if(!tracks.contains(ticker))tracks[ticker]=freshTrack();
        auto& st=tracks[ticker];
        need(st["history"].is_array()&&st["history"].size()<=600&&st["chart"].is_array()&&st["chart"].size()<=120,"Malformed or oversized history");
        r["reference"]=nullptr;r["reference_low"]=nullptr;r["reference_high"]=nullptr;r["gap"]=nullptr;r["robust_z"]=nullptr;
        r["candidate"]=false;r["basis"]="history";r["monitor_status"]="observing";r["edges"]=J::array();r["best"]=nullptr;r["issues"]=r["warnings"];
        r["reference_note"]="Prior rolling median; not an independent forecast";
        I stamp=r.value("captured_ms",I(0)),closeAt=utcMillis(str(r,"close_time"));r["close_ms"]=closeAt;
        bool good=r.value("valid",false)&&!r.value("stale",true)&&r.contains("mid")&&!r["mid"].is_null();
        if(!closeAt||closeAt<=now){good=false;r["issues"].push_back("Close time missing, unsupported or already reached");}
        if(r.contains("spread")&&!r["spread"].is_null()&&num(r,"spread")>c.maxSpread){good=false;r["issues"].push_back("Spread exceeds quality limit");}
        if(!r.contains("mid")||r["mid"].is_null())r["issues"].push_back("Two-sided book required for monitoring");
        const auto& market=record.at("market");
        std::string rules=hash(str(market,"rules_primary")+str(market,"rules_secondary")+str(market,"close_time"));
        if(st.value("rules",rules)!=rules){st=freshTrack();r["issues"].push_back("Rules changed; historical baseline reset");}
        st["rules"]=rules;
        if(st.value("last",I(0))>0&&stamp-st["last"].get<I>()>static_cast<I>(c.maxGap*1000)){st=freshTrack();st["rules"]=rules;r["issues"].push_back("Long capture gap; baseline reset");}
        if(stamp<=st.value("last",I(0))){good=false;r["issues"].push_back("Duplicate or out-of-order observation");}
        auto& h=st["history"];
        while(!h.empty()&&now-h.front().at("t").get<I>()>static_cast<I>(c.lookback*1000))h.erase(h.begin());
        r["samples"]=h.size();r["required_samples"]=c.minSamples;
        bool ready=h.size()>=static_cast<size_t>(c.minSamples)&&!h.empty()&&stamp-h.front().at("t").get<I>()>=static_cast<I>(c.minSpan*1000);
        bool anomaly=false;int direction=0;double reference=0,low=0,high=0;
        if(good) {
            double mid=num(r,"mid");J point={{"t",stamp},{"mid",mid}};
            if(ready) {
                std::vector<double> values;for(const auto& x:h){double v=numeric(x.at("mid"));need(v>=0&&v<=1,"Invalid historical probability");values.push_back(v);}
                reference=median(values);std::vector<double> deviations;for(double v:values)deviations.push_back(std::abs(v-reference));
                double scale=std::max({1.4826*median(deviations),c.sigmaFloor,num(r,"spread")/2});
                low=std::max(0.0,reference-2*scale);high=std::min(1.0,reference+2*scale);
                double gap=reference-mid,z=gap/scale;anomaly=std::abs(z)>=c.zThreshold&&std::abs(gap)>=c.minMove;direction=gap>0?1:-1;
                r["reference"]=reference;r["reference_low"]=low;r["reference_high"]=high;r["gap"]=gap;r["robust_z"]=z;
                if(anomaly){
                    if(st.value("direction",0)!=direction){st["quarantine"]=J::array();st["streak"]=0;}
                    st["direction"]=direction;st["streak"]=st.value("streak",0)+1;st["quarantine"].push_back(point);
                    r["monitor_status"]=st["streak"].get<int>()>=c.confirm?"confirmed_move":"unconfirmed_move";
                    r["edges"]=monitorEdges(r,reference,low,high,c);
                    if(st["streak"].get<int>()>=c.rebase){
                        h=st["quarantine"];st["quarantine"]=J::array();st["streak"]=0;r["monitor_status"]="baseline_reset";
                        r["issues"].push_back("Persistent move may reflect new information; old reference retired");
                    } else if(st["streak"].get<int>()>=c.confirm&&!r["edges"].empty()&&num(r["edges"][0],"lower_edge")>=c.minEdge)r["candidate"]=true;
                } else {st["streak"]=0;st["direction"]=0;st["quarantine"]=J::array();h.push_back(point);r["monitor_status"]="normal";}
            } else {h.push_back(point);st["streak"]=0;st["quarantine"]=J::array();r["monitor_status"]="warming_up";}
            while(h.size()>static_cast<size_t>(c.window))h.erase(h.begin());
            st["last"]=stamp;
            if(c.forecasts.contains(ticker)) {
                                                                                                     
                r["basis"]="model";r["candidate"]=false;r["edges"]=J::array();r["monitor_status"]="model_unavailable";
                r["reference"]=nullptr;r["reference_low"]=nullptr;r["reference_high"]=nullptr;r["gap"]=nullptr;r["reference_note"]="Supplied estimate unavailable";
                try {
                    const auto& f=c.forecasts[ticker];double p=numeric(f.at("probability")),lo=numeric(f.at("lower")),hi=numeric(f.at("upper"));
                    I asof=f.at("as_of_ms").get<I>(),until=f.at("valid_until_ms").get<I>();
                    need(lo>=0&&lo<=p&&p<=hi&&hi<=1,"Invalid forecast range");
                    need(asof>0&&asof<=now&&until>now&&until>asof,"Forecast expired or future-dated");
                    need(!str(f,"source").empty(),"Forecast source required");
                    if(f.contains("rules_hash"))need(str(f,"rules_hash")==rules,"Forecast rules hash mismatch");
                    r["reference"]=p;r["reference_low"]=lo;r["reference_high"]=hi;r["gap"]=p-mid;r["reference_note"]=str(f,"source");r["forecast_expires_ms"]=until;
                    r["edges"]=monitorEdges(r,p,lo,hi,c);r["monitor_status"]="model_no_edge";
                    if(!r["edges"].empty()&&num(r["edges"][0],"lower_edge")>=c.minEdge){
                        std::string side=str(r["edges"][0],"side"),sig=hash(J{{"p",p},{"lo",lo},{"hi",hi},{"source",str(f,"source")}}.dump());
                        if(st.value("model_side","")!=side||st.value("model_signature","")!=sig)st["model_streak"]=0;
                        st["model_side"]=side;st["model_signature"]=sig;st["model_streak"]=st.value("model_streak",0)+1;
                        r["candidate"]=st["model_streak"].get<int>()>=c.confirm;r["monitor_status"]=r["candidate"].get<bool>()?"model_candidate":"model_confirming";
                    } else st["model_streak"]=0;
                } catch(const std::exception& e){st["model_streak"]=0;r["issues"].push_back(e.what());}
            } else st["model_streak"]=0;
            point["reference"]=r["reference"];point["flagged"]=anomaly;st["chart"].push_back(point);while(st["chart"].size()>120)st["chart"].erase(st["chart"].begin());
        } else {r["monitor_status"]="quality_blocked";st["streak"]=0;st["model_streak"]=0;st["quarantine"]=J::array();}
        if(!r["edges"].empty())r["best"]=r["edges"][0];
        if(!r.value("fee",J::object()).value("known",false)){r["candidate"]=false;r["issues"].push_back("Unknown fee schedule: no net-edge ranking");}
        r["confirmations"]=r["basis"]=="model"?st.value("model_streak",0):st.value("streak",0);
        r["rules_hash"]=rules;r["history"]=st["chart"];rows.push_back(r);
    }
                                                                           
    for(auto it=tracks.begin();it!=tracks.end();++it)if(!seen.count(it.key())){it.value()["streak"]=0;it.value()["model_streak"]=0;it.value()["quarantine"]=J::array();}
    std::stable_sort(rows.begin(),rows.end(),[](const J& a,const J& b){
        if(a["candidate"]!=b["candidate"])return a["candidate"].get<bool>();
        double av=a["best"].is_null()?-2:num(a["best"],"lower_edge"),bv=b["best"].is_null()?-2:num(b["best"],"lower_edge");return av>bv;
    });
    return {{"schema","kalshilens.monitor.v1"},{"generated_ms",now},{"generated_at",utc(now)},{"synthetic",synthetic},{"source",snapshot.value("source","")},
        {"coverage",snapshot.value("coverage",J::object())},{"quantity",c.settings.quantity/100.0},{"max_age_seconds",c.maxAge},{"min_edge",c.minEdge},{"buffer",c.buffer},
        {"markets",rows},{"note","Historical reference gaps are uncalibrated mean-reversion hypotheses. Model gaps depend on supplied forecast ranges. Neither is guaranteed profit."}};
}
inline int anomalyCommand(int argc,char** argv) {
    std::map<std::string,std::string> opts;
    for(int i=2;i<argc;i+=2){need(i+1<argc,"Missing anomaly option value");std::string k=argv[i];need(k=="--in"||k=="--state"||k=="--config"||k=="--out","Unknown anomaly option");need(!opts.count(k),"Duplicate option");opts[k]=argv[i+1];}
    need(opts.count("--in")&&opts.count("--state")&&opts.count("--out"),"anomaly requires --in snapshot.json --state state.json --out output.json");
    auto config=monitorConfig(opts.count("--config")?readJson(opts["--config"]):J::object());
    J state=fs::exists(opts["--state"])?readJson(opts["--state"]):emptyMonitorState();
    auto result=scoreMonitor(readJson(opts["--in"]),state,config);
    writeJson(opts["--out"],result);writeJson(opts["--state"],state);return 0;
}
}                
