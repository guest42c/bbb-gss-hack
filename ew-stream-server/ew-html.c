
#include "config.h"

#include "ew-html.h"

#include <string.h>


#define BASE "/"

void
ew_html_header (GString *s, const char *title)
{
  g_string_append_printf(s,
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "<html>\n"
      "<head>\n"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
      "<title>%s</title>\n"
      "<style>\n"
      "body {background-color: #998276; font-family: Verdana, Geneva, sans-serif;}\n"
      "div#container {width: 812px; background-image: url(" BASE "images/template_bodybg.png); background-repeat: repeat-y;}\n"
      "div#nav {text-align: center; margin: 0 auto;}\n"
      "div#nav div {display: inline; margin: 0 -1px;}\n"
      "div#nav img {padding: 0; margin: 0;}\n"
      "div#content {margin: 0 30px;}\n"
      "form {font-size: 10pt;}\n"
      "fieldset {margin: 10px 0;}\n"
      "legend {color: #282a8c; font-weight: bold;}\n"
      "input, textarea {background: -webkit-gradient(linear, left top, left bottom, from(#edeaea), to(#fff)); background: -moz-linear-gradient(top, #edeaea, #fff);}\n"
      "table.subtab {margin-left: 15px;}\n"
      ".indent {margin-left: 15px;}\n"
      "</style>\n"
      "<!--[if IE 7]>\n"
      "<link rel=\"stylesheet\" href=\"ie7.css\" type=\"text/css\" />\n"
      "<![endif]-->\n"
      "</head>\n"
      "<body>\n"
      "<div id=\"container\">\n", title);
}

void
ew_html_footer (GString *s, const char *session_id)
{
  g_string_append (s,
      "<br />\n"
      "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/stream\">Live Stream</a><br />\n"
      "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/\">Available Streams</a><br />\n");
  if (session_id) {
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/admin/admin?session_id=%s\">Admin Interface</a><br />\n",
        session_id);
    g_string_append_printf (s,
        "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/logout?session_id=%s\">Log Out</a><br />\n",
        session_id);
  }

  g_string_append (s,
      "<div id=\"footer\">\n"
      "<a href=\"http://entropywave.com/\">\n"
      "<img src=\"" BASE "images/template_footer.png\" width=\"812\" height=\"97\" border=\"0\" alt=\"\" /></div><!-- end footer div -->\n"
      "</a>\n"
      "</div><!-- end container div -->\n"
      "</body>\n"
      "</html>\n"); 

}

void
ew_html_error_404 (SoupMessage *msg)
{
  char *content;
  GString *s;

  s = g_string_new ("");
  
  g_string_append (s,
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "<html>\n"
      "<head>\n"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"   
      "<title>Error 404: Page not found</title>\n"
      "</head>\n"
      "Error 404: Page not found\n"
      "<body>\n"
      "</body>\n"
      "</html>\n");

  content = g_string_free (s, FALSE);

  soup_message_set_response (msg, "text/html", SOUP_MEMORY_TAKE,
      content, strlen(content));

  soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
}

