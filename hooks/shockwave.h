#ifndef CLS_HOOK_SHOCKWAVE_H
#define CLS_HOOK_SHOCKWAVE_H

#include "cls_hook.h"

#include <QStringList>

#include <utility>
#include <vector>

/**
 * Adobe Shockwave / Director players: the standalone Shockwave player
 * (SPR.exe, as used by Flashpoint) and Director projectors. These are 32-bit
 * Windows programs, so the guest address space is the process's own 32-bit
 * address space, mapped 1:1. Lingo values move around the heap as Director
 * reallocates its value pools, so pointer chains should start in a module
 * that is loaded at its preferred base, such as Dirapi.dll at 0x68000000.
 *
 * Shockwave movies have no product codes, so they are identified by the URL
 * of the movie the player was launched with. Players under Flashpoint and in
 * browsers are always given the original web address even when the file is
 * served locally, so the URL is the same across installs. The normalized URL
 * is sent as the product code, with version "0". A movie launched from a
 * local file is hashed normally.
 */
class ClsHookShockwave : public ClsHook
{
public:
  ClsHookShockwave(unsigned pid, const cls_window_preset_t *preset, void *window) :
    ClsHook(pid, preset, window) {}

  bool init(void) override;

  bool getIdentification(cl_game_identifier_t *identifier) override;

  const char *getLibrary(void) override { return "shockwave"; }

  /**
   * Normalizes a movie URL so that equivalent addresses identify the same
   * movie: lowercased, without the scheme, query, fragment, port or a
   * leading "www.", and with forward slashes.
   * ie. "http://www.candystand.com/dkbarrel/dkbmload.dcr?x=1" becomes
   * "candystand.com/dkbarrel/dkbmload.dcr"
   * @return The normalized URL, or an empty string if it isn't a web address
   */
  static QString normalizeUrl(const QString &url);

private:
  /** @return The command line arguments of the hooked process */
  QStringList arguments(void);

  /**
   * @return The start and end of each run of writable memory the process has
   * mapped below 4GB, with touching mappings merged, in address order
   */
  std::vector<std::pair<uintptr_t, uintptr_t>> writableSpans(void);
};

#endif
