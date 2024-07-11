//
// Copyright (c) 2024 Devin Smith <devin@devinsmith.net>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include <fx.h>

#include "Config.h"
#include "QueryTool.h"
#include "SqlConnection.h"

int main(int argc, char *argv[])
{
  // By default, programs start in the "C" locale, set to "" to prefer
  // the user-preferred locale.
  setlocale(LC_ALL, "");

  if (!Config::instance().load()) {
    // An error is issued from Config::load
    return -1;
  }

  tds::sql_startup(nullptr);

  FXApp app("querytool", "drs");
  app.init(argc, argv);

  new QueryTool(&app); // Deleted by FXTopWindow::Close
  app.create();

  app.run();

  return 0;
}
