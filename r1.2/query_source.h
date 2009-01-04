/* Copyright (C) 2007 MySQL AB

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/


class query_source {
 protected:
  request_rec *r;
 public:
  int req_method;
  const char *content_type;
  bool keep_tx_open;
  apr_table_t *form_data;
  virtual int get_form_data() = 0;
  virtual ~query_source() {};
    
  void *operator new(size_t sz, ap_pool *p) {
    return ap_pcalloc(p, sz);
  }

};


class HTTP_query_source : public query_source {
 public:
  HTTP_query_source(request_rec *req) {
    r = req;
    req_method = r->method_number;
    content_type = ap_table_get(r->headers_in, "Content-Type");
    keep_tx_open = false;
    form_data = ap_make_table(r->pool, 6);
  };
    
  int get_form_data() {
    return read_request_body(r, &form_data, content_type);
  };
  
};


class Apache_subrequest_query_source : public query_source {
  public:
  Apache_subrequest_query_source(request_rec *);
  int get_form_data();
};