/* Copyright (c) 2013-2014 the Civetweb developers
 * Copyright (c) 2013 No Face Press, LLC
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

// Simple example program on how to use Embedded C++ interface.

#include "CivetServer.h"

#include "lm.h"
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define DOCUMENT_ROOT "."
#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"
bool exitNow = false;

static const char* html_style=
		"<style>"
		"table {width:100%;border:1px solid #ccc;}"
		"tr:nth-child(odd){background-color:#fff;}"
		"tr:nth-child(even){background-color:#f1f1f1;}"
		// "tr:hover{background-color:#707070;}"
		"</style>";


class ExampleHandler : public CivetHandler
{
public:
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		mg_printf(conn,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/html\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "<html><body>\r\n");
		mg_printf(conn,
				"<h2>This is an example text from a C++ handler</h2>\r\n");
		mg_printf(conn,
				"<p>To see a page from the A handler <a "
				"href=\"a\">click here</a></p>\r\n");
		mg_printf(conn,
				"<p>To see a page from the A handler with a parameter "
				"<a href=\"a?param=1\">click here</a></p>\r\n");
		mg_printf(conn,
				"<p>To see a page from the A/B handler <a "
				"href=\"a/b\">click here</a></p>\r\n");
		mg_printf(conn,
				"<p>To see a page from the *.foo handler <a "
				"href=\"xy.foo\">click here</a></p>\r\n");
		mg_printf(conn,
				"<p>To exit <a href=\"%s\">click here</a></p>\r\n",
				EXIT_URI);
		mg_printf(conn, "</body></html>\r\n");
		return true;
	}
};

class ExitHandler : public CivetHandler
{
public:
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		mg_printf(conn,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/plain\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "Bye!\n");
		exitNow = true;
		return true;
	}
};

class LmHtmlHandler : public CivetHandler
{
private:
	bool
	handleAll(const char *method,
			CivetServer *server,
			struct mg_connection *conn)
	{
		std::string s = "";
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		using namespace lm;
		Peek* p;
		p=Peek::open(req_info->uri+5);
		if(p!=NULL)
		{
			if(isdir(p->flags()))
			{
				mg_printf(conn,
						"HTTP/1.1 200 OK\r\nContent-Type: "
						"text/html\r\nConnection: close\r\n\r\n");
				mg_printf(conn,
						"<html><head>%s</head><body>",html_style);
				mg_printf(conn, "<h2>%s</h2>", req_info->uri+5);
				if (CivetServer::getParam(conn, "param", s)) {
					mg_printf(conn, "<p>param set to %s</p>", s.c_str());
				} else {
					mg_printf(conn, "<p>param not set</p>");
				}

				mg_printf(conn,
						"<p>The request was:<br><pre>%s :%s: HTTP/%s</pre></p>\n",
						req_info->request_method,
						req_info->uri,
						req_info->http_version);

				mg_printf(conn, "<table>");
				//if dir is not top, add ".."
				std::string up = req_info->uri;
				up = up.substr(0, up.rfind('/'));
				if (up.size() >= 5)
				  mg_printf(conn,
				            "<tr>"
				            "<td style=\"height:20px;width:20px\">"
				            "<img src=\"/img/folder.png\"  style=\"display:inline;height:100%%;width:auto;\" >"
				            "</td>"
				            "<td><a href=\"%s\">..</a></td></tr>\n",
				            up.c_str()
				  );
				std::vector< dir_item_d > s;
				p->list(s);
				for (size_t n = 0; n<s.size(); n++)
				{
					const char* icon;
					if (isdir(s[n].flags)) icon = "/img/folder.png";
					else if (isreadable(s[n].flags)) icon = "/img/file.png";
					else icon = "/img/file.png";
					mg_printf(conn,
							"<tr>"
							"<td style=\"height:20px;width:20px\">"
							"<img src=\"%s\"  style=\"display:inline;height:100%%;width:auto;\" >"
							"</td>"
							"<td><a href=\"%s\">%s</a></td></tr>\n",
							icon,
							(std::string(req_info->uri)+"/"+s[n].name).c_str(),
							s[n].name.c_str()
					);
				}
				mg_printf(conn, "</table></body></html>\n");
			}
			else
			{
				if(isreadable(p->flags()))
				{
					mg_printf(conn,
							"HTTP/1.1 200 OK\r\n"
							"Content-Type: text/plain\r\n"
							"Connection: close\r\n"
							"\r\n");
					result_e r;
					std::string data;
					do
					{
						r=p->read(data);
						if(r==Success||r==MoreData)
						{
							mg_write(conn,data.c_str(),data.size());
						}
					}
					while(r==MoreData);
				}
			}
		}

		return true;
	}

public:
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		return handleAll("GET", server, conn);
	}
	bool
	handlePost(CivetServer *server, struct mg_connection *conn)
	{
		return handleAll("POST", server, conn);
	}
};


class ImageHandler : public CivetHandler
{
	const char* content_type;
	const uint8_t* image;
	size_t size;
public:
	ImageHandler(const char* content_type, const uint8_t* image, size_t size)
:content_type(content_type), image(image), size(size)
{
}
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		mg_printf(conn,
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"\r\n",
				content_type,
				(int)size);
		mg_write(conn, image, size);
		return true;
	}
};


class ABHandler : public CivetHandler
{
public:
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		mg_printf(conn,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/html\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "<html><body>");
		mg_printf(conn, "<h2>This is the AB handler!!!</h2>");
		mg_printf(conn, "</body></html>\n");
		return true;
	}
};

class FooHandler : public CivetHandler
{
public:
	bool
	handleGet(CivetServer *server, struct mg_connection *conn)
	{
		/* Handler may access the request info using mg_get_request_info */
		const struct mg_request_info *req_info = mg_get_request_info(conn);

		mg_printf(conn,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/html\r\nConnection: close\r\n\r\n");

		mg_printf(conn, "<html><body>\n");
		mg_printf(conn, "<h2>This is the Foo GET handler!!!</h2>\n");
		mg_printf(conn,
				"<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>\n",
				req_info->request_method,
				req_info->uri,
				req_info->http_version);
		mg_printf(conn, "</body></html>\n");

		return true;
	}
	bool
	handlePost(CivetServer *server, struct mg_connection *conn)
	{
		/* Handler may access the request info using mg_get_request_info */
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		long long rlen, wlen;
		long long nlen = 0;
		long long tlen = req_info->content_length;
		char buf[1024];

		mg_printf(conn,
				"HTTP/1.1 200 OK\r\nContent-Type: "
				"text/html\r\nConnection: close\r\n\r\n");

		mg_printf(conn, "<html><body>\n");
		mg_printf(conn, "<h2>This is the Foo POST handler!!!</h2>\n");
		mg_printf(conn,
				"<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>\n",
				req_info->request_method,
				req_info->uri,
				req_info->http_version);
		mg_printf(conn, "<p>Content Length: %li</p>\n", (long)tlen);
		mg_printf(conn, "<pre>\n");

		while (nlen < tlen) {
			rlen = tlen - nlen;
			if (rlen > sizeof(buf)) {
				rlen = sizeof(buf);
			}
			rlen = mg_read(conn, buf, rlen);
			if (rlen <= 0) {
				break;
			}
			wlen = mg_write(conn, buf, rlen);
			if (rlen != rlen) {
				break;
			}
			nlen += wlen;
		}

		mg_printf(conn, "\n</pre>\n");
		mg_printf(conn, "</body></html>\n");

		return true;
	}

#define fopen_recursive fopen

	bool
	handlePut(CivetServer *server, struct mg_connection *conn)
	{
		/* Handler may access the request info using mg_get_request_info */
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		long long rlen, wlen;
		long long nlen = 0;
		long long tlen = req_info->content_length;
		FILE * f;
		char buf[1024];
		int fail = 0;

		snprintf(buf, sizeof(buf), "D:\\somewhere\\%s\\%s", req_info->remote_user, req_info->local_uri);
		buf[sizeof(buf)-1] = 0; /* TODO: check overflow */
		f = fopen_recursive(buf, "wb");

		if (!f) {
			fail = 1;
		} else {
			while (nlen < tlen) {
				rlen = tlen - nlen;
				if (rlen > sizeof(buf)) {
					rlen = sizeof(buf);
				}
				rlen = mg_read(conn, buf, (size_t)rlen);
				if (rlen <= 0) {
					fail = 1;
					break;
				}
				wlen = fwrite(buf, 1, (size_t)rlen, f);
				if (rlen != rlen) {
					fail = 1;
					break;
				}
				nlen += wlen;
			}
			fclose(f);
		}

		if (fail) {
			mg_printf(conn,
					"HTTP/1.1 409 Conflict\r\n"
					"Content-Type: text/plain\r\n"
					"Connection: close\r\n\r\n");
			//MessageBeep(MB_ICONERROR);
		} else {
			mg_printf(conn,
					"HTTP/1.1 201 Created\r\n"
					"Content-Type: text/plain\r\n"
					"Connection: close\r\n\r\n");
			//MessageBeep(MB_OK);
		}

		return true;
	}
};
#include <stdint.h>
lm::result_e echo_instance_read(lm::CTX* ctx, std::string& value)
{
	char s[10];
	sprintf(s,"%d",(int)(uintptr_t)ctx->common);
	value=s;
	return lm::Success;
}

extern "C" uint8_t _binary_folder_png_start;
extern "C" uint8_t _binary_folder_png_end;
extern "C" uint8_t _binary_file_png_start;
extern "C" uint8_t _binary_file_png_end;

int
main(int argc, char *argv[])
{

	using namespace lm;

	CTX ctx={0};
	ctx.read=echo_instance_read;
	int result=0;
	int i,j;
	for(j=0;j<2*3*5*7*11*13;j++)
	{
		i=j*17;
		char s[30];
		sprintf(s,"%d/%d/%d/%d/%d/%d",i%2,i%3,i%5,i%7,i%11,i%13);
		ctx.common=(void*)(uintptr_t)(i%(2*3*5*7*11*13));
		if(register_node(s,&ctx)!=0)
		{
			result=-1000000-i;
			break;
		}
	}
	const char *options[] = {
			//"document_root", DOCUMENT_ROOT,
			"listening_ports", PORT, 0};

	CivetServer server(options);

	ExampleHandler h_ex;
	server.addHandler(EXAMPLE_URI, h_ex);

	ExitHandler h_exit;
	server.addHandler(EXIT_URI, h_exit);

	//AHandler h_a;
	//server.addHandler("/a", h_a);

	ABHandler h_ab;
	server.addHandler("/a/b", h_ab);

	ImageHandler folder("image/png",&_binary_folder_png_start, &_binary_folder_png_end-&_binary_folder_png_start);
	server.addHandler("/img/folder.png", folder);
	ImageHandler file("image/png",&_binary_file_png_start, &_binary_file_png_end-&_binary_file_png_start);
	server.addHandler("/img/file.png", file);

	LmHtmlHandler h_xxx;
	server.addHandler("/html", h_xxx);

	printf("Browse files at http://localhost:%s/\n", PORT);
	printf("Run example at http://localhost:%s%s\n", PORT, EXAMPLE_URI);
	printf("Exit at http://localhost:%s%s\n", PORT, EXIT_URI);

	while (!exitNow) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	printf("Bye!\n");

	return 0;
}
