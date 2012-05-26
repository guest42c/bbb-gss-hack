/* GStreamer Streaming Server
 * Copyright (C) 2009-2012 Entropy Wave Inc <info@entropywave.com>
 * Copyright (C) 2009-2012 David Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#include "config.h"

#include "gss-html.h"
#include "gss-server.h"
#include "gss-soup.h"

#include <string.h>


void
gss_html_header (GssServer * server, GString * s, const char *title)
{
  g_string_append_printf (s,
#ifdef USE_XHTML
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
#else
#ifdef USE_HTML5
      "<!DOCTYPE html>\n"
#else
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
#endif
      "<html>\n"
#endif
      "<head>\n"
#ifdef USE_XHTML
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
#else
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
#endif
      "<title>%s</title>\n", title);

  if (server->append_style_html) {
    server->append_style_html (server, s, server->append_style_html_priv);
  }

  g_string_append (s, "</head>\n" "<body>\n" "<div id=\"container\">\n");
}

void
gss_html_footer (GssServer * server, GString * s, const char *session_id)
{
  gss_html_append_break (s);
  g_string_append (s,
      "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/\">Available Streams</a>\n");
  if (session_id) {
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/admin/admin?session_id=%s\">Admin Interface</a>\n",
        session_id);
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/logout?session_id=%s\">Log Out</a>\n",
        session_id);
  }
  gss_html_append_break (s);

  g_string_append (s, "<div id=\"footer\">\n");
  g_string_append (s, "<a href=\"http://entropywave.com/\">\n");
  gss_html_append_image (s, "/images/template_footer.png", 812, 97, NULL);
  g_string_append (s, "</a>");
  g_string_append (s, "</div><!-- end footer div -->\n");
  g_string_append (s,
      "</div><!-- end container div -->\n" "</body>\n" "</html>\n");

}

void
gss_html_error_404 (SoupMessage * msg)
{
  char *content;
  GString *s;

  s = g_string_new ("");

  g_string_append (s,
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "<head>\n"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
      "<title>Error 404: Page not found</title>\n"
      "</head>\n"
      "<body>\n" "Error 404: Page not found\n" "</body>\n" "</html>\n");

  content = g_string_free (s, FALSE);

  soup_message_set_response (msg, "text/html", SOUP_MEMORY_TAKE,
      content, strlen (content));

  soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
}

void
gss_html_append_break (GString * s)
{
#ifdef USE_XHTML
  g_string_append (s, "<br />");
#else
  g_string_append (s, "<br>");
#endif
}

void
gss_html_append_image (GString * s, const char *url, int width, int height,
    const char *alt_text)
{
  if (alt_text == NULL)
    alt_text = "";

  g_string_append_printf (s, "<img src='%s' alt='%s' ", url,
      alt_text ? alt_text : "");
  if (width > 0 && height > 0) {
    g_string_append_printf (s, "width='%d' height='%d' ", width, height);
  }
#ifdef USE_HTML5
  /* border is in CSS */
#else
  g_string_append (s, "border='0' ");
#endif

#ifdef USE_XHTML
  g_string_append (s, "/>");
#else
#ifdef USE_HTML5
  g_string_append (s, "/>");
#else
  g_string_append (s, ">");
#endif
#endif
}

void
gss_html_append_image_printf (GString * s, const char *url_format, int width,
    int height, const char *alt_text, ...)
{
  va_list args;
  char *url;

  va_start (args, alt_text);
  url = g_strdup_vprintf (url_format, args);
  va_end (args);

  gss_html_append_image (s, url, width, height, alt_text);

  g_free (url);
}


void
gss_html_bootstrap_doc (GssTransaction * t)
{
  int i;
  GString *s = t->s;

  g_string_append (s,
      "<!DOCTYPE html>\n"
      "<html lang='en'>\n"
      "  <head>\n"
      "    <meta charset='utf-8'>\n"
      "    <title>Entropy Wave Streaming Server</title>\n"
      "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
#if 0
      "    <meta name='description' content=''>\n"
      "    <meta name='author' content=''>\n"
#endif
      "    <link href='/bootstrap/css/bootstrap.css' rel='stylesheet'>\n"
      "    <style type='text/css'>\n"
      "      body {\n"
      "        padding-top: 60px;\n"
      "        padding-bottom: 40px;\n"
      "      }\n"
      "      .sidebar-nav {\n"
      "        padding: 9px 0;\n"
      "      }\n"
      "    </style>\n"
      "    <link href='/bootstrap/css/bootstrap-responsive.css' rel='stylesheet'>\n"
#if 0
      "    <!--[if lt IE 9]>\n"
      "      <script src='http://html5shim.googlecode.com/svn/trunk/html5.js'></script>\n"
      "    <![endif]-->\n"
#endif
      "    <script src=\"include.js\" type=\"text/javascript\"></script>\n"
#if 0
      "    <link rel='shortcut icon' href='/favicon.ico'>\n"
      "    <link rel='apple-touch-icon-precomposed' sizes='144x144' href='../assets/ico/apple-touch-icon-144-precomposed.png'>\n"
      "    <link rel='apple-touch-icon-precomposed' sizes='114x114' href='../assets/ico/apple-touch-icon-114-precomposed.png'>\n"
      "    <link rel='apple-touch-icon-precomposed' sizes='72x72' href='../assets/ico/apple-touch-icon-72-precomposed.png'>\n"
      "    <link rel='apple-touch-icon-precomposed' href='../assets/ico/apple-touch-icon-57-precomposed.png'>\n"
#endif
      "  </head>\n"
      "  <body>\n"
      "    <div class='navbar navbar-fixed-top'>\n"
      "      <div class='navbar-inner'>\n"
      "        <div class='container-fluid'>\n"
      "          <a class='btn btn-navbar' data-toggle='collapse' data-target='.nav-collapse'>\n"
      "            <span class='icon-bar'></span>\n"
      "            <span class='icon-bar'></span>\n"
      "            <span class='icon-bar'></span>\n"
      "          </a>\n"
      "          <a class='brand' href='#'>Entropy Wave Streaming Server</a>\n"
      "          <div class='btn-group pull-right'>\n");

  if (t->session) {
    g_string_append_printf (s,
        "            <a class='btn dropdown-toggle' data-toggle='dropdown' data-target='#'>\n"
        "              <i class='icon-user'></i> %s\n"
        "              <span class='caret'></span>\n", t->session->username);
  } else {
    g_string_append_printf (s,
        "<a href='%s/login' title='Administrative Interface'>Admin</a>\n"
        "<a href='#' id='browserid' title='Sign-in with BrowserID'>\n"
        "<img src='/images/sign_in_blue.png' alt='Sign in' onclick='navigator.id.get(gotAssertion);'>\n"
        "</a>\n", gss_soup_get_base_url_https (t->server, t->msg));
  }

  g_string_append (s,
      "            </a>\n"
      "            <ul class='dropdown-menu'>\n"
      "              <li><a href='#'>Profile</a></li>\n"
      "              <li class='divider'></li>\n"
      "              <li><a href='#'>Sign Out</a></li>\n"
      "            </ul>\n"
      "          </div>\n"
      "          <div class='nav-collapse'>\n"
      "            <ul class='nav'>\n"
      "              <li class='active'><a href='#'>Home</a></li>\n"
      "              <li><a href='#about'>About</a></li>\n"
      "              <li><a href='#contact'>Contact</a></li>\n"
      "            </ul>\n"
      "          </div><!--/.nav-collapse -->\n"
      "        </div>\n"
      "      </div>\n"
      "    </div>\n"
      "    <div class='container-fluid'>\n"
      "      <div class='row-fluid'>\n"
      "        <div class='span3'>\n"
      "          <div class='well sidebar-nav'>\n"
      "            <ul class='nav nav-list'>\n"
      "              <li class='nav-header'>Programs</li>\n");
  for (i = 0; i < t->server->n_programs; i++) {
    GssProgram *program = t->server->programs[i];
    g_string_append_printf (s,
        "              <li><a href='/%s'>%s</a></li>\n",
        program->location, program->location);
  };

  if (t->session) {
    g_string_append (s,
        "              <li class='nav-header'>User</li>\n"
        "              <li><a href='#'>Add Program</a></li>\n"
        "              <li><a href='#'>Main</a></li>\n"
        "              <li><a href='#'>Log</a></li>\n");
  }
  if (t->session && t->session->is_admin) {
    g_string_append (s,
        "              <li class='nav-header'>Administration</li>\n"
        "              <li><a href='#'>Access Control</a></li>\n"
        "              <li><a href='#'>Password</a></li>\n"
        "              <li><a href='#'>Certificate</a></li>\n"
        "              <li class='nav-header'>Other</li>\n"
        "              <li><a href='#'>Monitor</a></li>\n"
        "              <li><a href='#'>Meep</a></li>\n");
  }
  g_string_append (s,
      "            </ul>\n"
      "          </div><!--/.well -->\n"
      "        </div><!--/span-->\n" "        <div class='span9'>\n");

  g_string_append (s,
      "          <div class='hero-unit'>\n"
      "              <div style='background-color:#000000;color:#ffffff;width:320px;height:180px;text-align:center;'>currently unavailable</div>\n"
      "            <p>Content #1.</p>\n"
      "            <p><a class='btn btn-primary btn-large'>Learn more &raquo;</a></p>\n"
      "          </div>\n");

#if 0
  g_string_append (s,
      "          <div class='row-fluid'>\n"
      "            <div class='span4'>\n"
      "              <h2>Heading</h2>\n"
      "              <p>Content #2. </p>\n"
      "              <p><a class='btn' href='#'>View details &raquo;</a></p>\n"
      "            </div><!--/span-->\n"
      "            <div class='span4'>\n"
      "              <h2>Heading</h2>\n"
      "              <p>Content #2. </p>\n"
      "              <p><a class='btn' href='#'>View details &raquo;</a></p>\n"
      "            </div><!--/span-->\n"
      "            <div class='span4'>\n"
      "              <h2>Heading</h2>\n"
      "              <p>Content #2. </p>\n"
      "              <p><a class='btn' href='#'>View details &raquo;</a></p>\n"
      "            </div><!--/span-->\n" "          </div><!--/row-->\n");
#endif

  g_string_append (s,
      "        </div><!--/span-->\n"
      "      </div><!--/row-->\n" "      <hr>\n" "      <footer>\n");

  if (t->server->footer_html) {
    t->server->footer_html (t->server, s, t->server->footer_html_priv);
  } else {
    g_string_append (s,
        "        <div class='span4'>\n"
        "          <p>&copy; Entropy Wave Inc 2012</p>\n" "        </div>\n");
  }

  g_string_append (s,
      "      </footer>\n"
      "    </div><!--/.fluid-container-->\n"
      "    <script src='/bootstrap/js/jquery.js'></script>\n"
      "    <script src='/bootstrap/js/bootstrap.js'></script>\n");

  g_string_append_printf (s,
      "<script type=\"text/javascript\">\n"
      "function gotAssertion(assertion) {\n"
      "if(assertion!==null){\n"
      "var form = document.createElement(\"form\");\n"
      "form.setAttribute('method', 'POST');\n"
      "form.setAttribute('action', '%s/login');\n"
      "var ip = document.createElement(\"input\");\n"
      "ip.setAttribute('type', 'hidden');\n"
      "ip.setAttribute('name', 'assertion');\n"
      "ip.setAttribute('value', assertion);\n"
      "form.appendChild(ip);\n"
      "document.body.appendChild(form);\n" "form.submit();\n"
      //"document.body.removeChild(form);\n"
      "}\n"
      "}\n" "</script>\n", gss_soup_get_base_url_https (t->server, t->msg));
  g_string_append (s, "\n" "  </body>\n" "</html>\n");

}
