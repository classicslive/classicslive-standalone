#include "infuse.h"

#include <QInputDialog>

#include <string.h>

typedef struct
{
  const char *product;
  const char *filename;
} zeebo_game_t;

static const zeebo_game_t games[] =
{
  { "274214", "Crash Bandicoot Nitro Kart 3D" },
  { "274754", "DOUBLE DRAGON Zeebo" },
  { "277229", "Zeebo Family Pack" },

/*
  { "263019", "Ultimate Chess 3D" },
  { "274259", "Action Hero 3D" },

  { "274791", "Zeebo App (aparentemente não tem função)" },
  { "274802", "Quake" },
  { "274803", "FIFA 09" },
  { "274804", "Treino Cerebral" },

  { "276121", "Need For Speed: Carbon - Domine a Cidade" },
  { "276152", "Ridge Racer" },
  { "276153", "Quake 2" },
  { "276154", "Prey Evil" },
  { "276212", "Pac-Mania" },
  { "276675", "Resident Evil 4" },
  { "276731", "Tekken 2" },
  { "276809", "Zeebo Extreme Rolimã" },

  { "277083", "Bejeweled Twist" },
  { "277380", "Galaxy on Fire" },
  { "277455", "Zenonia" },
  { "277495", "Zeebo Channels (Opera Mini)" },
  { "277534", "Zeebo Sports Tenis" },

  { "278200", "Heavy Weapon" },
  { "278212", "Zeebo Sports Vôlei" },
  { "278282", "Rally Master Pro" },
  { "278283", "Zeebo Extreme Jetboard" },
  { "278285", "Zeebo Extreme Bóia Cross" },
  { "278738", "Zeebo Sports Queimada" },
  { "278962", "Peggle" },
  { "278965", "Toy Raid" },
  { "278986", "Caveman Ninja" },
  { "278987", "Spinmaster" },
  { "278988", "Street Hoop" },

  { "279036", "Um Jogo de Ovos" },
  { "279125", "Super Burger Time" },
  { "279126", "Karnov’s Revenge" },
  { "279159", "Zeebo Sports Peteca" },
  { "279173", "Wizard Fire" },
  { "279200", "Magical Drop 3" },
  { "279233", "Dark Seal" },
  { "279369", "Alien Breaker Deluxe" },
  { "279380", "Zeebo F.C. Foot Camp" },
  { "279382", "Zeeboids" },
  { "279394", "Zeebo Clube" },
  { "279712", "Zuma's Revenge" },
  { "279888", "Bad Dudes vs. Dragon Ninja" },

  { "280173", "All Star Cards" },
  { "280214", "Armageddon Squadron" },
  { "280221", "Iron Sight" },
  { "280238", "Powerboat Challenge" },
  { "280386", "Alice no País das Maravilhas" },
  { "280394", "Reckless Racing" },
  { "280463", "Tork and Krall" },
  { "280602", "Raging Thunder 2" },
  { "280634", "Turma da Mônica em: Vamos Brincar Vol.1" },
  { "280647", "Zeebo F.C. Super League" }
*/
};

bool ClsHookInfuse::getIdentification(cl_game_identifier_t *identifier)
{
  if (!identifier)
    return false;

  // Build a QStringList for Qt selection dialog
  QStringList gameNames;
  for (const auto &g : games)
    gameNames << QString("%1 - %2").arg(g.product).arg(g.filename);

  // Ask user
  bool ok = false;
  QString choice = QInputDialog::getItem(
    nullptr,
    "Zeebo Game Selection",
    "Which game are you playing?",
    gameNames,
    0,            // default index
    false,        // editable? no
    &ok
  );

  if (!ok || choice.isEmpty())
    return false;

  // Find selected game
  int index = gameNames.indexOf(choice);
  if (index < 0)
    return false;

  const zeebo_game_t &selected = games[index];

  // Fill the identifier
  identifier->type = CL_GAMEIDENTIFIER_PRODUCT_CODE;
  strncpy(identifier->product, selected.product, sizeof(identifier->product));
  strncpy(identifier->filename, selected.filename, sizeof(identifier->filename));
  strncpy(identifier->version, "1", sizeof(identifier->version));

  return true;
}


bool ClsHookInfuse::init(void)
{
  static const cls_find_memory_region_t fmr =
  {
#if CL_HOST_PLATFORM == CL_PLATFORM_WINDOWS
    .host_offset=0x40,
#elif CL_HOST_PLATFORM == CL_PLATFORM_LINUX
    .host_offset=0x10,
#endif
    .host_size=0xA001000,
    .guest_base=0,
    .guest_size=0xA000000,
    .endianness=CL_ENDIAN_LITTLE,
    .pointer_size=4,
    .title = "128MB + 32MB DDR SDRAM"
  };

  return ClsHook::init() && initViaMemoryRegions(fmr);
}

bool ClsHookInfuse::run(void)
{
  return true;
}
