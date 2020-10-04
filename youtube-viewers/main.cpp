#include <mutex>
#include <random>
#include <Windows.h>
#include "curl/curl.h"
#include <thread>
#include <string>
#include <vector>
#include <tuple>
#include <istream>
#include <ostream>
#include <fstream>

#pragma comment(lib, "libcurl.a")
#pragma comment(lib, "libcurldll.a")

int curlProxyType;
std::string streamUrl;
std::mutex g_mutex;
std::vector<std::string> proxyarr;
std::vector<std::thread::id> threads;
const int threadTimeout = 5000;
int threadCount;

typedef size_t(*CURL_WRITEFUNCTION_PTR)(void*, size_t, size_t, void*);
size_t write_to_string(void *ptr, size_t size, size_t count, void *stream) {
	static_cast<std::string*>(stream)->append(static_cast<char*>(ptr), 0, size*count);
	return size * count;
}

std::string parseString(std::string before, std::string after, std::string source)
{
	if (before.empty())
		return"";
	if (after.empty())
		return "";
	if (source.empty())
		return "";
	if (source.find(before) == std::string::npos)
		return "";
	if (source.find(after) == std::string::npos)
		return "";

	std::string t = strstr(source.c_str(), before.c_str());
	t.erase(0, before.length());
	std::string::size_type loc = t.find(after, 0);
	t = t.substr(0, loc);
	return t;
}

std::string replaceAll(std::string source, std::string from, std::string to)
{
	if (from.empty())
		return "";
	if (source.empty())
		return"";
	
	size_t nPos = source.find(from);

	while(nPos != std::string::npos)
	{
		source.replace(nPos, nPos + from.size(), to);
		nPos = source.find(from);		
	}
	
	return source;
}

std::string randomString(size_t len)
{
	std::string str("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");

	while (str.size() > len)
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, str.size());
		int pos = dis(gen);
		str.erase(pos, 1);
	}

	return str;
}


std::tuple<std::string, std::string, long> sendRequest(std::string requestAddr, std::string requestProxy, std::vector<std::string> requestHeader)
{
	try
	{
		CURL *curl = curl_easy_init();

		struct curl_slist *header = NULL;

		for (auto& elem : requestHeader)
			header = curl_slist_append(header, elem.c_str());

		curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
		curl_easy_setopt(curl, CURLOPT_URL, requestAddr.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, static_cast<CURL_WRITEFUNCTION_PTR>(write_to_string));
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, static_cast<CURL_WRITEFUNCTION_PTR>(write_to_string));
		
		std::string content;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);

		
		std::string headers;
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
		curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
		if (!requestProxy.empty())
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, requestProxy.c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYTYPE, curlProxyType);
		}

		const auto res = curl_easy_perform(curl);

		long response_code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		curl_easy_cleanup(curl);
		
		if (res == CURLE_OK && response_code == 200)
			return std::make_tuple(content, headers, response_code);
		
		return std::make_tuple("", "", -1);
	}
	catch (...)
	{
		return std::make_tuple("", "", -1);
	}
	
}

void threadLoop(std::string proxy)
{
	while (!g_mutex.try_lock()) Sleep(100);
	threads.push_back(std::this_thread::get_id());
	g_mutex.unlock();

	
	auto[body, header, code] = sendRequest("https://m.youtube.com/watch?v=" + streamUrl, proxy, { "Accept-Language: ru",
		"Keep-Alive: true",
		"User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 13_3_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0.5 Mobile/15E148 Safari/604.16" });

	if (code == 200)
	{
		std::string cookie = parseString("Set-Cookie: ", "Alt-Svc:", header);
		cookie = replaceAll(cookie, "Set-Cookie:", "");
		cookie = replaceAll(cookie, "\r\n", "");

		std::string text1 = parseString("ptracking?ei=", "\\", body);
		std::string text2 = parseString("INNERTUBE_CONTEXT_CLIENT_VERSION\":\"", "\"", body);
		std::string text3 = parseString("watchtime?cl=", "\\", body);
		std::string text4 = parseString("of=", "\\", body);
		std::string text5 = parseString("vm=", "\\", body);
		std::string text6 = parseString("live=", "\\", body);

		while (true)
		{

			auto[body2, header2, code2] = sendRequest("https://m.youtube.com/api/stats/watchtime?ns=yt&el=detailpage&cpn=" + randomString(16) +
				"&docid=" + streamUrl +
				"&ver=2&referrer=https%3A%2F%2Fm.youtube.com%2F&ei=" + text1 +
				"&fmt=136&fs=0&rt=1&of=" + text4 +
				"&euri&live=" + text6 +
				"&cl=" + text3 +
				"&state=playing&vm=" + text5 +
				"&volume=100&c=MWEB&cver=" + text2 +
				"&cplayer=UNIPLAYER&cbrand=apple&cbr=Safari%20Mobile&cbrver=13.0.5.15E148&cmodel=iphone&cos=iPhone&cosver=13_3_1&cplatform=MOBILE&delay=5&hl=ru_RU&cr=RU&rtn=600&feature=c4-overview&rti=1&muted=0",
				proxy, { "User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 13_3_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0.5 Mobile/15E148 Safari/604.16", "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8", "Cookie:" + cookie });

			//todo add check alive proxy
			
			Sleep(threadTimeout);
		}
	}
	
	while (!g_mutex.try_lock()) Sleep(1000);
	auto it = std::find(threads.begin(), threads.end(), std::this_thread::get_id());

	if (it != threads.end())
		threads.erase(it);

	g_mutex.unlock();

	printf("[INFO] Proxy %s is down\n", proxy.c_str());
}

void updateTitle()
{
	while (true)
	{
		std::string title = "Threads Active: " + std::to_string(threads.size()) + " Proxy left: " + std::to_string(proxyarr.size());
		SetConsoleTitleA(title.c_str());
		Sleep(1000);
	}
}

int main()
{
	threadCount = 1000; //count of threads
	streamUrl = "xxyyzz"; //https://www.youtube.com/watch?v=xxyyzz
	curlProxyType = CURLPROXY_SOCKS4; //proxy type
	
	std::ifstream in("./proxies.txt", std::ios::in);

	if (in.is_open())
	{
		g_mutex.lock();
		std::string line;
		while (getline(in, line))
			proxyarr.push_back(line);
		in.close();
		g_mutex.unlock();

		printf("[INFO] Loaded %i proxies\n", proxyarr.size());
	}
	else
	{
		printf("[ERROR] Proxies not found\n");
		return 0;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	std::thread(updateTitle).detach();
	
	while(true)
	{
		if (threadCount < threads.size())
			continue;

		if (proxyarr.empty())
			break;

		std::string proxy = proxyarr.front();
		proxyarr.erase(proxyarr.begin());

		std::thread(threadLoop, proxy).detach();
	}

	printf("[INFO] Proxy list ended\n");
	getchar();
	
	curl_global_cleanup();
	
	return 0;
}