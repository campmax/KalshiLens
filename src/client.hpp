#pragma once
#include "core.hpp"
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib,"winhttp.lib")
#else
#include <curl/curl.h>
#endif
namespace kl {
namespace fs=std::filesystem;
inline std::string encode(const std::string& s){const char* hex="0123456789ABCDEF";std::string out;
 for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')out+=c;
 else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
inline void writeJson(const fs::path& p,const J& j){std::ofstream f(p,std::ios::binary);need(bool(f),"Cannot write "+p.string());f<<j.dump(2);need(bool(f),"Write failed "+p.string());}
inline J readJson(const fs::path& p){need(fs::file_size(p)<=64*1024*1024,"JSON file exceeds 64 MiB");std::ifstream f(p,std::ios::binary);need(bool(f),"Cannot read "+p.string());return J::parse(f);}
struct HttpResult{long status=0;std::string body;};
class HttpClient {
 fs::path raw_;int delay_=300,requests_=0;I previous_=0;public:J audit=J::array();
 explicit HttpClient(fs::path raw,int delay=300):raw_(std::move(raw)),delay_(delay){fs::create_directories(raw_);
#ifndef _WIN32
 need(curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK,"libcurl initialization failed");
#endif
 }
 ~HttpClient(){
#ifndef _WIN32
 curl_global_cleanup();
#endif
 }
 HttpClient(const HttpClient&)=delete;HttpClient& operator=(const HttpClient&)=delete;
 HttpResult transport(const std::string& path){
  need(path.rfind("/",0)==0&&path.find('\r')==std::string::npos&&path.find('\n')==std::string::npos,"Invalid API path");
  need(path.rfind("/markets",0)==0||path.rfind("/events/",0)==0||path.rfind("/series/",0)==0,"Endpoint outside read-only allowlist");
  HttpResult out;
#ifdef _WIN32
  struct Handle{HINTERNET h=nullptr;explicit Handle(HINTERNET v):h(v){}~Handle(){if(h)WinHttpCloseHandle(h);}Handle(const Handle&)=delete;};
  Handle session(WinHttpOpen(L"KalshiLens/1.0 public-research",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  need(session.h!=nullptr,"WinHTTP session failed");WinHttpSetTimeouts(session.h,5000,5000,15000,15000);
  Handle connection(WinHttpConnect(session.h,L"external-api.kalshi.com",INTERNET_DEFAULT_HTTPS_PORT,0));need(connection.h!=nullptr,"WinHTTP connection failed");
  std::string endpoint="/trade-api/v2"+path;std::wstring w(endpoint.begin(),endpoint.end());
  Handle request(WinHttpOpenRequest(connection.h,L"GET",w.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
  need(request.h!=nullptr,"WinHTTP request failed");DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
  need(WinHttpSendRequest(request.h,L"Accept: application/json\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)!=0,"HTTPS send failed");
  need(WinHttpReceiveResponse(request.h,nullptr)!=0,"HTTPS response failed");DWORD status=0,size=sizeof(status);
  need(WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=0,"Missing HTTP status");out.status=status;
  while(true){DWORD available=0;need(WinHttpQueryDataAvailable(request.h,&available)!=0,"HTTPS read failed");if(!available)break;
   need(out.body.size()+available<=20*1024*1024,"API response exceeds 20 MiB");std::string chunk(available,'\0');DWORD read=0;
   need(WinHttpReadData(request.h,chunk.data(),available,&read)!=0,"HTTPS body read failed");out.body.append(chunk.data(),read);}
#else
  CURL* c=curl_easy_init();need(c!=nullptr,"libcurl handle failed");
  std::string url="https://external-api.kalshi.com/trade-api/v2"+path;
  curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_USERAGENT,"KalshiLens/1.0 public-research");
  curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,5L);curl_easy_setopt(c,CURLOPT_TIMEOUT,20L);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
  curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,+[](char* ptr,size_t size,size_t n,void* target)->size_t{
   auto& body=*static_cast<std::string*>(target);size_t bytes=size*n;if(body.size()+bytes>20*1024*1024)return 0;body.append(ptr,bytes);return bytes;});
  curl_easy_setopt(c,CURLOPT_WRITEDATA,&out.body);auto code=curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&out.status);curl_easy_cleanup(c);
  need(code==CURLE_OK,std::string("HTTPS error: ")+curl_easy_strerror(code));
#endif
  return out;
 }
 J get(const std::string& path){
  for(int attempt=0;attempt<3;++attempt){I gap=nowMs()-previous_;if(gap<delay_)std::this_thread::sleep_for(std::chrono::milliseconds(delay_-gap));
   I started=nowMs();HttpResult r;
   try{r=transport(path);}catch(const std::exception& e){audit.push_back({{"path",path},{"error",e.what()},{"at",utc(nowMs())}});throw;}
   previous_=nowMs();++requests_;J log={{"path",path},{"status",r.status},{"received_at",utc(previous_)},{"duration_ms",previous_-started},{"attempt",attempt+1}};
   auto filename=std::to_string(requests_)+".json";log["raw_file"]=filename;audit.push_back(log);
   J parsed;
   try{parsed=J::parse(r.body);writeJson(raw_/filename,{{"request",log},{"response",parsed}});}
   catch(const J::exception&){need(false,"API returned non-JSON HTTP "+std::to_string(r.status));}
   if(r.status==200){need(parsed.is_object(),"API response must be object");return parsed;}
   if((r.status==429||r.status>=500)&&attempt<2){std::this_thread::sleep_for(std::chrono::seconds(1<<attempt));continue;}
   throw std::runtime_error("API HTTP "+std::to_string(r.status)+" for "+path+". No credentials are requested; see request audit.");
  }throw std::runtime_error("API retries exhausted");
 }
};
struct CollectOptions{std::string series,event,ticker;int pages=2,maxMarkets=24;};
inline J collect(const CollectOptions& opt,const std::function<J(const std::string&)>& get){
 I started=nowMs();J markets=J::array();std::set<std::string> seen,cursors;std::string cursor;bool truncated=false;int pages=0;
 if(!opt.ticker.empty())markets.push_back(get("/markets/"+encode(opt.ticker)).at("market"));
 else for(int i=0;i<opt.pages;++i){
  std::string path="/markets?limit=100&status=open&mve_filter=exclude";
  if(!opt.series.empty())path+="&series_ticker="+encode(opt.series);
  if(!opt.event.empty())path+="&event_ticker="+encode(opt.event);
  if(!cursor.empty())path+="&cursor="+encode(cursor);
  J page=get(path);++pages;need(page.contains("markets")&&page["markets"].is_array(),"Missing markets array");
  for(const auto& m:page["markets"]){std::string t=str(m,"ticker");need(!t.empty(),"Missing ticker");if(seen.insert(t).second)markets.push_back(m);}
  cursor=str(page,"cursor");if(cursor.empty())break;need(cursors.insert(cursor).second,"Repeated pagination cursor");
  if(i+1==opt.pages)truncated=true;
 }
 need(!markets.empty(),"No markets returned. Try another series or remove your filter.");
 size_t discovered=markets.size();
 std::stable_sort(markets.begin(),markets.end(),[](const J& a,const J& b){return num(a,"volume_24h_fp")>num(b,"volume_24h_fp");});
 if(markets.size()>static_cast<size_t>(opt.maxMarkets)){markets.erase(markets.begin()+opt.maxMarkets,markets.end());truncated=true;}
 std::map<std::string,J> events,series;J records=J::array();
 for(const auto& m:markets){J r={{"market",m},{"event",J::object()},{"series",J::object()},{"orderbook",J::object()},{"errors",J::array()}};
  std::string e=str(m,"event_ticker");
  try{if(!events.count(e)){auto eventResponse=get("/events/"+encode(e));events[e]=eventResponse.at("event");events[e].erase("markets");}
   r["event"]=events[e];std::string s=str(events[e],"series_ticker");need(!s.empty(),"Series ticker missing");
   if(!series.count(s))series[s]=get("/series/"+encode(s)).at("series");
   r["series"]=series[s];
  }catch(const std::exception& ex){r["errors"].push_back(std::string("Metadata: ")+ex.what());}
  try{I requestStart=nowMs();r["orderbook"]=get("/markets/"+encode(str(m,"ticker"))+"/orderbook");
   r["received_ms"]=nowMs();r["book_request_ms"]=nowMs()-requestStart;
  }catch(const std::exception& ex){r["errors"].push_back(std::string("Orderbook: ")+ex.what());r["received_ms"]=I(0);}
  records.push_back(r);std::cout<<"  "<<records.size()<<"/"<<markets.size()<<"  "<<str(m,"ticker")<<'\n';
 }
 return {{"schema","kalshilens.snapshot.v1"},{"synthetic",false},{"source","Kalshi public REST / external-api.kalshi.com"},
 {"started_at",utc(started)},{"completed_at",utc(nowMs())},{"records",records},
 {"coverage",{{"discovered",discovered},{"selected",markets.size()},{"pages",pages},{"truncated",truncated},
 {"series_filter",opt.series},{"event_filter",opt.event},{"ticker_filter",opt.ticker},
 {"selection","Highest 24-hour volume within the scanned pages; NOT an exchange-wide top-volume ranking"}}}};
}
inline J demoSnapshot(){I t=nowMs();J records=J::array();
 const std::vector<std::string> titles={"Aurora mission launches this month?","Harbor team wins the final?","Valley team wins the final?","Summit team wins the final?",
 "City temperature exceeds 80 degrees?","City temperature exceeds 90 degrees?","New battery achieves its target?","One-sided example market?","Unknown fee schedule example?","Crossed-book example?","Thin liquidity example?","Archived observation example?"};
 for(size_t i=0;i<titles.size();++i){std::string ticker="DEMO-"+std::to_string(i+1),event=i>=1&&i<=3?"DEMO-FINAL":"DEMO-E"+std::to_string(i+1);
  I yes=4000+static_cast<I>(i%5)*500,no=10000-yes-200;if(i>=1&&i<=3){yes=4000;no=5800;}if(i==9)no=10000-yes+100;
  J y=J::array(),n=J::array();for(int k=0;k<8;++k){I q=i==10?25:1000+(k+1)*1500;
   y.push_back(J::array({decimal(yes-k*100,4),decimal(q,2)}));n.push_back(J::array({decimal(no-k*100,4),decimal(q+100,2)}));}
  if(i==7)y=J::array();
  J m={{"ticker",ticker},{"event_ticker",event},{"title",titles[i]},{"yes_sub_title","Illustrative binary outcome"},
    {"market_type","binary"},{"notional_value_dollars","1.0000"},{"status","active"},{"close_time","2030-01-01T00:00:00Z"},
    {"last_price_dollars",decimal(yes+100,4)},{"volume_fp",decimal(250000+static_cast<I>(i)*71200,2)},
    {"volume_24h_fp",decimal(70000+static_cast<I>(i)*8200,2)},{"open_interest_fp","3500.00"},
    {"rules_primary","Invented demonstration contract. Binary YES pays $1; NO pays $0 when the condition is true."},
    {"rules_secondary","No live market or real settlement source. All names, quotes and activity are synthetic."}};
  J e={{"event_ticker",event},{"series_ticker","DEMO-SERIES"},{"mutually_exclusive",i>=1&&i<=3}};
  J s={{"ticker","DEMO-SERIES"},{"category",i%2==0?"Science":"Sports"},{"fee_type",i==8?"flat":"quadratic"},{"fee_multiplier",1}};
  records.push_back({{"market",m},{"event",e},{"series",s},{"orderbook",{{"orderbook_fp",{{"yes_dollars",y},{"no_dollars",n}}}}},
   {"received_ms",i==11?t-120000:t-static_cast<I>(i)*50},{"book_request_ms",25},{"errors",J::array()}});
 }
 return {{"schema","kalshilens.snapshot.v1"},{"synthetic",true},{"source","SYNTHETIC DEMONSTRATION - no real Kalshi quotes"},
 {"started_at",utc(t)},{"completed_at",utc(t)},{"records",records},{"coverage",{{"selected",records.size()},{"discovered",records.size()},{"truncated",false},{"selection","Invented fixtures including deliberate data-quality failures"}}}};
}
inline J compare(const J& before,const J& after){std::map<std::string,J> old;for(const auto& r:before.at("markets"))old[str(r,"ticker")]=r;
 J rows=J::array();for(const auto& a:after.at("markets")){auto t=str(a,"ticker");if(!old.count(t))continue;const auto& b=old[t];
  if(!a.contains("mid")||a["mid"].is_null()||!b.contains("mid")||b["mid"].is_null())continue;
  rows.push_back({{"ticker",t},{"before_mid",b["mid"]},{"after_mid",a["mid"]},{"change",a["mid"].get<double>()-b["mid"].get<double>()},
   {"before_spread",b["spread"]},{"after_spread",a["spread"]},{"before_captured_ms",b["captured_ms"]},{"after_captured_ms",a["captured_ms"]}});
 }return {{"schema","kalshilens.comparison.v1"},{"rows",rows},{"note","Observed quote changes only; no attribution of intent or proof of manipulation."}};
}
}                
