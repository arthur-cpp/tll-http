// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

// Based on IANA registry: https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml

#ifndef _HTTP_STATUS_H
#define _HTTP_STATUS_H

#include <string_view>

static constexpr std::string_view http_status_string(unsigned short code)
{
	switch (code) {
		case 100: return "100 Continue";
		case 101: return "101 Switching Protocols";
		case 102: return "102 Processing";
		case 103: return "103 Early Hints";
		case 104: return "104 Upload Resumption Supported";
		case 200: return "200 OK";
		case 201: return "201 Created";
		case 202: return "202 Accepted";
		case 203: return "203 Non-Authoritative Information";
		case 204: return "204 No Content";
		case 205: return "205 Reset Content";
		case 206: return "206 Partial Content";
		case 207: return "207 Multi-Status";
		case 208: return "208 Already Reported";
		case 226: return "226 IM Used";
		case 300: return "300 Multiple Choices";
		case 301: return "301 Moved Permanently";
		case 302: return "302 Found";
		case 303: return "303 See Other";
		case 304: return "304 Not Modified";
		case 305: return "305 Use Proxy";
		case 307: return "307 Temporary Redirect";
		case 308: return "308 Permanent Redirect";
		case 400: return "400 Bad Request";
		case 401: return "401 Unauthorized";
		case 402: return "402 Payment Required";
		case 403: return "403 Forbidden";
		case 404: return "404 Not Found";
		case 405: return "405 Method Not Allowed";
		case 406: return "406 Not Acceptable";
		case 407: return "407 Proxy Authentication Required";
		case 408: return "408 Request Timeout";
		case 409: return "409 Conflict";
		case 410: return "410 Gone";
		case 411: return "411 Length Required";
		case 412: return "412 Precondition Failed";
		case 413: return "413 Content Too Large";
		case 414: return "414 URI Too Long";
		case 415: return "415 Unsupported Media Type";
		case 416: return "416 Range Not Satisfiable";
		case 417: return "417 Expectation Failed";
		case 421: return "421 Misdirected Request";
		case 422: return "422 Unprocessable Content";
		case 423: return "423 Locked";
		case 424: return "424 Failed Dependency";
		case 425: return "425 Too Early";
		case 426: return "426 Upgrade Required";
		case 428: return "428 Precondition Required";
		case 429: return "429 Too Many Requests";
		case 431: return "431 Request Header Fields Too Large";
		case 451: return "451 Unavailable For Legal Reasons";
		case 500: return "500 Internal Server Error";
		case 501: return "501 Not Implemented";
		case 502: return "502 Bad Gateway";
		case 503: return "503 Service Unavailable";
		case 504: return "504 Gateway Timeout";
		case 505: return "505 HTTP Version Not Supported";
		case 506: return "506 Variant Also Negotiates";
		case 507: return "507 Insufficient Storage";
		case 508: return "508 Loop Detected";
		case 510: return "510 Not Extended";
		case 511: return "511 Network Authentication Required";
		default: return "";
	}
}

#endif// _HTTP_STATUS_H
