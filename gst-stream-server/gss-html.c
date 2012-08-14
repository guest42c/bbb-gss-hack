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
gss_html_error_404 (GssServer * server, SoupMessage * msg)
{
  char *content;
  GString *s;
  GssTransaction t = { 0 };

  s = g_string_new ("");

  t.s = s;
  t.server = server;
  t.msg = msg;
  gss_html_header (&t);
  g_string_append (s, "<h1>Error 404: Page not found</h1>\n");
  gss_html_footer (&t);

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
gss_html_header (GssTransaction * t)
{
  GString *s = t->s;
  gchar *session_id;
  GList *g;
  char *safe_title;

  if (t->session) {
    session_id = g_strdup_printf ("?session_id=%s", t->session->session_id);
  } else {
    session_id = g_strdup ("");
  }

  safe_title = gss_html_sanitize_entity (t->server->title);
  g_string_append_printf (s,
      "<!DOCTYPE html>\n"
      "<html lang='en'>\n"
      "  <head>\n"
      "    <meta charset='utf-8'>\n" "    <title>%s</title>\n", safe_title);

  g_string_append_printf (s,
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
      "          <a class='brand' href='/%s'>%s</a>\n"
      "          <div class='btn-group pull-right'>\n", session_id, safe_title);
  g_free (safe_title);

  if (t->session) {
    g_string_append_printf (s,
        "            <a class='btn dropdown-toggle' data-toggle='dropdown' data-target='#'>\n"
        "              <i class='icon-user'></i> %s\n"
        "              <span class='caret'></span>\n", t->session->username);
  } else {
#if 0
    char *base_url = gss_soup_get_base_url_https (t->server, t->msg);
    g_string_append_printf (s,
        "<a href='%s/login' title='Administrative Interface'>Admin</a>\n",
        base_url);
    g_free (base_url);
#endif
    g_string_append_printf (s,
        "<a href='#' id='browserid' title='Sign-in with Persona'>\n"
        "<img src='/sign_in_blue.png' alt='Sign in' onclick='navigator.id.get(gotAssertion);'>\n"
        "</a>\n");
  }

  g_string_append_printf (s,
      "            </a>\n"
      "            <ul class='dropdown-menu'>\n"
      "              <li><a href='/profile%s'>Profile</a></li>\n"
      "              <li class='divider'></li>\n"
      "              <li><a href='/logout%s'>Sign Out</a></li>\n"
      "            </ul>\n"
      "          </div>\n"
      "          <div class='nav-collapse'>\n"
      "            <ul class='nav'>\n"
      "              <li class='active'><a href='/%s'>Home</a></li>\n"
      "              <li><a href='/about%s'>About</a></li>\n"
      "              <li><a href='/contact%s'>Contact</a></li>\n"
      "            </ul>\n"
      "          </div><!--/.nav-collapse -->\n"
      "        </div>\n"
      "      </div>\n"
      "    </div>\n"
      "    <div class='container-fluid'>\n"
      "      <div class='row-fluid'>\n"
      "        <div class='span3'>\n"
      "          <div class='well sidebar-nav'>\n",
      session_id, session_id, session_id, session_id, session_id);
  g_string_append_printf (s, "            <ul class='nav nav-list'>\n");
  if (t->server->featured_resources) {
    g_string_append_printf (s,
        "              <li class='nav-header'>Featured Pages</li>\n");
    for (g = t->server->featured_resources; g; g = g_list_next (g)) {
      GssResource *resource = g->data;
      g_string_append_printf (s,
          "              <li %s><a href='%s%s'>%s</a></li>\n",
          (resource == t->resource) ? "class='active'" : "",
          resource->location, session_id, resource->name);
    };
  }
  g_string_append_printf (s,
      "              <li class='nav-header'>Programs</li>\n");
  for (g = t->server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;
    if (program->is_archive)
      continue;
    g_string_append_printf (s,
        "              <li %s><a href='%s%s'>%s</a></li>\n",
        (program->resource == t->resource) ? "class='active'" : "",
        program->resource->location, session_id, GST_OBJECT_NAME (program));
  };

  g_string_append_printf (s,
      "              <li class='nav-header'>Archive</li>\n");
  for (g = t->server->programs; g; g = g_list_next (g)) {
    GssProgram *program = g->data;
    if (!program->is_archive)
      continue;
    g_string_append_printf (s,
        "              <li %s><a href='%s%s'>%s</a></li>\n",
        (program->resource == t->resource) ? "class='active'" : "",
        program->resource->location, session_id, GST_OBJECT_NAME (program));
  };

  if (t->session) {
    g_string_append_printf (s,
        "              <li class='nav-header'>User</li>\n"
        "              <li><a href='/add_program%s'>Add Program</a></li>\n"
        "              <li><a href='/dashboard%s'>Dashboard</a></li>\n",
        session_id, session_id);
  }
  if (t->session && t->session->is_admin) {
    GList *g;

    g_string_append (s,
        "              <li class='nav-header'>Administration</li>\n");

    for (g = t->server->admin_resources; g; g = g_list_next (g)) {
      GssResource *r = (GssResource *) g->data;
      g_string_append_printf (s,
          "              <li %s><a href='%s%s'>%s</a></li>\n",
          (r == t->resource) ? "class='active'" : "",
          r->location, session_id, r->name);
    }
  }
  g_string_append (s,
      "            </ul>\n"
      "          </div><!--/.well -->\n"
      "        </div><!--/span-->\n" "        <div class='span9'>\n");

  g_free (session_id);
}

void
gss_html_bootstrap_doc (GssTransaction * t)
{
  GString *s = t->s;

  g_string_append (s,
      "          <div class='hero-unit'>\n"
      "              <img src='/offline.png'>\n"
      "            <p>Content #1.</p>\n"
      "            <p><a class='btn btn-primary btn-large'>Learn more &raquo;</a></p>\n"
      "          </div>\n");
}

void
gss_html_footer (GssTransaction * t)
{
  GString *s = t->s;

  g_string_append (s,
      "        </div><!--/span-->\n" "      </div><!--/row-->\n");

  if (t->server->footer_html) {
    t->server->footer_html (t->server, s, t->server->footer_html_priv);
  } else {
    g_string_append (s,
        "        <div class='span4'>\n"
        "          <p>&copy; Entropy Wave Inc 2012</p>\n" "        </div>\n");
  }

  g_string_append (s,
      "    </div><!--/.fluid-container-->\n"
      "    <script src='/bootstrap/js/jquery.js'></script>\n"
      "    <script src='/bootstrap/js/bootstrap.js'></script>\n"
      //"    <script src=\"/include.js\" type=\"text/javascript\"></script>\n"
      "    <script src=\"https://login.persona.org/include.js\" type=\"text/javascript\"></script>\n");

  g_string_append (s, "<script type=\"text/javascript\">\n");
  g_string_append_printf (s,
      "function gotAssertion(assertion) {\n"
      "if(assertion!==null){\n"
      "var form = document.createElement(\"form\");\n"
      "form.setAttribute('method', 'POST');\n"
      "form.setAttribute('action', '%s/login?redirect_url=%s');\n"
      "var ip = document.createElement(\"input\");\n"
      "ip.setAttribute('type', 'hidden');\n"
      "ip.setAttribute('name', 'assertion');\n"
      "ip.setAttribute('value', assertion);\n"
      "form.appendChild(ip);\n"
      "document.body.appendChild(form);\n" "form.submit();\n"
      "}\n" "}\n",
      gss_soup_get_base_url_https (t->server, t->msg), t->path ? t->path : "/");
  if (t->script) {
    g_string_append (s, t->script->str);
    g_string_free (t->script, TRUE);
  }
  g_string_append (s, "</script>\n");
  g_string_append (s, "\n" "  </body>\n" "</html>\n");

}


static const char hexchar[16] = "0123456789abcdef";

char *
gss_html_sanitize_attribute (const char *s)
{
  int escape_count;
  char *out;
  char *t;
  int len;
  int i;

  g_return_val_if_fail (s != NULL, NULL);

  len = strlen (s);
  escape_count = 0;
  for (i = 0; i < len; i++) {
    if (!g_ascii_isalnum (s[i]))
      escape_count++;
  }

  out = g_malloc (len + escape_count * 5 + 1);
  t = out;
  for (i = 0; i < len; i++) {
    if (g_ascii_isalnum (s[i])) {
      t[0] = s[i];
      t++;
    } else {
      /* &#xHH; */
      t[0] = '&';
      t[1] = '#';
      t[2] = 'x';
      t[3] = hexchar[(s[i] >> 4) & 0xf];
      t[4] = hexchar[s[i] & 0xf];
      t[5] = ';';
      t += 6;
    }
  }
  t[0] = 0;

  return out;
}

char *
gss_html_sanitize_entity (const char *s)
{
  int escape_count;
  char *out;
  char *t;
  int len;
  int i;

  len = strlen (s);
  escape_count = 0;
  for (i = 0; i < len; i++) {
    if (s[i] == '&' || s[i] == '<' || s[i] == '>' || s[i] == '"' ||
        s[i] == '\'' || s[i] == '/')
      escape_count++;
  }

  out = g_malloc (len + escape_count * 5 + 1);
  t = out;
  for (i = 0; i < len; i++) {
    if (s[i] == '&' || s[i] == '<' || s[i] == '>' || s[i] == '"' ||
        s[i] == '\'' || s[i] == '/') {
      /* &#xHH; */
      t[0] = '&';
      t[1] = '#';
      t[2] = 'x';
      t[3] = hexchar[(s[i] >> 4) & 0xf];
      t[4] = hexchar[s[i] & 0xf];
      t[5] = ';';
      t += 6;
    } else {
      t[0] = s[i];
      t++;
    }
  }
  t[0] = 0;

  return out;

}


char *
gss_html_sanitize_url (const char *s)
{
  int escape_count;
  char *out;
  char *t;
  int len;
  int i;

  len = strlen (s);
  escape_count = 0;
  for (i = 0; i < len; i++) {
    if (!g_ascii_isalnum (s[i]))
      escape_count++;
  }

  out = g_malloc (len + escape_count * 2 + 1);
  t = out;
  for (i = 0; i < len; i++) {
    if (g_ascii_isalnum (s[i])) {
      t[0] = s[i];
      t++;
    } else {
      t[0] = '%';
      t[1] = hexchar[(s[i] >> 4) & 0xf];
      t[2] = hexchar[s[i] & 0xf];
      t += 3;
    }
  }
  t[0] = 0;

  return out;
}

gboolean
gss_html_entity_is_sane (const char *s)
{
  int i;
  int len;

  len = strlen (s);
  for (i = 0; i < len; i++) {
    if (!g_ascii_isalnum (s[i])) {
      return FALSE;
    }
  }

  return TRUE;
}

gboolean
gss_html_attribute_is_sane (const char *s)
{
  int i;
  int len;

  len = strlen (s);
  for (i = 0; i < len; i++) {
    if (!g_ascii_isalnum (s[i])) {
      return FALSE;
    }
  }

  return TRUE;

}

gboolean
gss_html_url_is_sane (const char *s)
{
  int i;
  int len;

  len = strlen (s);
  for (i = 0; i < len; i++) {
    if (!g_ascii_isalnum (s[i])) {
      return FALSE;
    }
  }

  return TRUE;

}
