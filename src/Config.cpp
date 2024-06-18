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

#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>

#include "Config.h"

static bool config_mkdir(const char *path)
{
  struct stat st;

  if (stat(path, &st) != 0) {
    if (mkdir(path, 0700) != 0) {
      fprintf(stderr, "Error: mkdir %s: %s\n", path, strerror(errno));
      return false;
    }
  } else {
    if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "Error: %s is not a directory\n", path);
      return false;
    }
  }
  return true;
}

bool Config::load()
{
  // Determine home directory.
  const struct passwd *pw = getpwuid(getuid());
  if (pw == nullptr) {
    fprintf(stderr, "Fatal: Can't get your user info!\n");
    return false;
  }

  _conf_dir = pw->pw_dir;
  _conf_dir += "/.querytool";

  return config_mkdir(_conf_dir.text());
}

