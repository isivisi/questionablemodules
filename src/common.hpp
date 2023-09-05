#pragma once
#include <rack.hpp>
#include <vector>
#include <openssl/crypto.h>
#define CURL_STATICLIB
#include <curl/curl.h>

#include <network.hpp>
#include <system.hpp>
#include <asset.hpp>

using namespace rack;
using namespace network;

namespace q {
namespace network {
// modified from network.cpp

static const std::vector<std::string> methodNames = {
	"GET", "POST", "PUT", "DELETE",
};

static CURL* createCurl() {
	CURL* curl = curl_easy_init();
	assert(curl);

	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	std::string userAgent = APP_NAME + " " + APP_EDITION_NAME + "/" + APP_VERSION;
	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
	// Timeout to wait on initial HTTP connection.
	// This is lower than the typical HTTP timeout of 60 seconds to avoid DAWs from aborting plugin scans.
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);

	// If curl can't resolve a DNS entry, it sends a signal to interrupt the process.
	// However, since we use curl on non-main thread, this crashes the application.
	// So tell curl not to signal.
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	std::string caPath = asset::system("cacert.pem");
	curl_easy_setopt(curl, CURLOPT_CAINFO, caPath.c_str());

	return curl;
}

static int xferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	float* progress = (float*) clientp;
	if (progress) {
		if (dltotal <= 0)
			*progress = 0.f;
		else
			*progress = (float)dlnow / dltotal;
	}
	return 0;
}

static size_t writeStringCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
	std::string* str = (std::string*) userdata;
	size_t len = size * nmemb;
	str->append(ptr, len);
	return len;
}

static std::string getCookieString(const CookieMap& cookies) {
	std::string s;
	for (const auto& pair : cookies) {
		s += encodeUrl(pair.first);
		s += "=";
		s += encodeUrl(pair.second);
		s += ";";
	}
	return s;
}

// with header support >:(
json_t* requestJson(Method method, const std::string& url, json_t* dataJ, const std::vector<std::string>& customHeaders, const CookieMap& cookies) {
	std::string urlS = url;
	CURL* curl = createCurl();
	char* reqStr = NULL;

	// Process data
	if (dataJ) {
		if (method == METHOD_GET) {
			// Append ?key1=value1&key2=value2&... to url
			urlS += "?";
			bool isFirst = true;
			const char* key;
			json_t* value;
			json_object_foreach(dataJ, key, value) {
				if (json_is_string(value)) {
					if (!isFirst)
						urlS += "&";
					urlS += key;
					urlS += "=";
					const char* str = json_string_value(value);
					size_t len = json_string_length(value);
					char* escapedStr = curl_easy_escape(curl, str, len);
					urlS += escapedStr;
					curl_free(escapedStr);
					isFirst = false;
				}
			}
		}
		else {
			reqStr = json_dumps(dataJ, 0);
		}
	}

	curl_easy_setopt(curl, CURLOPT_URL, urlS.c_str());

	// Set HTTP method
	if (method == METHOD_GET) {
		// This is CURL's default
	}
	else if (method == METHOD_POST) {
		curl_easy_setopt(curl, CURLOPT_POST, true);
	}
	else if (method == METHOD_PUT) {
		curl_easy_setopt(curl, CURLOPT_PUT, true);
	}
	else if (method == METHOD_DELETE) {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	}

	// Set headers
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
    for (size_t i = 0; i < customHeaders.size(); i++) { headers = curl_slist_append(headers, customHeaders[i].c_str()); }
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	// Cookies
	if (!cookies.empty()) {
		curl_easy_setopt(curl, CURLOPT_COOKIE, getCookieString(cookies).c_str());
	}

	// Body callbacks
	if (reqStr)
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reqStr);

	std::string resText;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resText);

	// Perform request
	INFO("Requesting JSON %s %s", methodNames[method].c_str(), urlS.c_str());
	CURLcode res = curl_easy_perform(curl);

	// Cleanup
	if (reqStr)
		std::free(reqStr);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	if (res != CURLE_OK) {
		WARN("Could not request %s: %s", urlS.c_str(), curl_easy_strerror(res));
		return NULL;
	}

	// Parse JSON response
	json_error_t error;
	json_t* rootJ = json_loads(resText.c_str(), 0, &error);
	return rootJ;
}

bool requestDownload(const std::string& url, const std::string& filename, float* progress, const std::vector<std::string>& customHeaders, const CookieMap& cookies) {
	CURL* curl = createCurl();

	FILE* file = std::fopen(filename.c_str(), "wb");
	if (!file)
		return false;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, false);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progress);
	// Fail on 4xx and 5xx HTTP codes
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

    // Set headers
	struct curl_slist* headers = NULL;
    for (size_t i = 0; i < customHeaders.size(); i++) { headers = curl_slist_append(headers, customHeaders[i].c_str()); }
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	// Cookies
	if (!cookies.empty()) {
		curl_easy_setopt(curl, CURLOPT_COOKIE, getCookieString(cookies).c_str());
	}

	INFO("Requesting download %s", url.c_str());
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	std::fclose(file);

	if (res != CURLE_OK) {
		system::remove(filename);
		WARN("Could not download %s: %s", url.c_str(), curl_easy_strerror(res));
		return false;
	}

	return true;
}

}
}