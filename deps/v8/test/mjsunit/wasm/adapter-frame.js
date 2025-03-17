function describeEgyxosClothing(character) {
  const clothingDescriptions = {
    Kefer: {
      head: "A golden helmet with a stylized falcon crest.",
      torso: "A fitted, dark blue tunic with gold trim and a wide, ornate belt. Often features a pectoral with scarab or sun disk motifs.",
      legs: "Dark blue or black trousers, sometimes with gold bands or wrappings. Sandals or armored footwear.",
      arms: "Often bare or with decorative armbands. Sometimes includes segmented armor or gauntlets.",
      accessories: "May carry a khopesh or other weapons. Often adorned with amulets and jewelry.",
      overallStyle: "Regal and militaristic, emphasizing power and authority."
    },
    Exaton: {
      head: "A complex, segmented helmet with glowing red or blue elements. Often features a central crest or antenna.",
      torso: "Form-fitting, metallic armor with glowing energy conduits. The chest plate often features a central power source or symbol.",
      legs: "Segmented, robotic-looking leg armor. Often includes boosters or thrusters.",
      arms: "Robotic arms with integrated weapons or tools. May include energy blades or blasters.",
      accessories: "Energy packs, communication devices, and various technological enhancements.",
      overallStyle: "Highly technological and futuristic, emphasizing power and combat capabilities."
    },
    Apis: {
      head: "A bull-shaped helmet or headdress, often with golden horns and a decorative brow.",
      torso: "A muscular, exposed chest or a chest plate with bull-themed designs. Often features a wide, decorative belt.",
      legs: "Simple loincloth or short kilt. Sometimes includes greaves or sandals.",
      arms: "Often bare, emphasizing strength. May include simple armbands or bracelets.",
      accessories: "May carry a labrys (double-axe) or other blunt weapons. Often adorned with bull-shaped amulets.",
      overallStyle: "Brutal and powerful, emphasizing raw strength and animalistic ferocity."
    },
    Tutankhamun: {
        head: "A golden pharaoh's crown or headdress, often with a cobra and vulture motif. May also feature a nemes headdress with golden stripes.",
        torso: "A richly decorated golden pectoral and broad collar. Often includes a sheer linen tunic underneath, or a jeweled breastplate.",
        legs: "A pleated white linen kilt, often adorned with gold and jewels. Sandals or golden footwear.",
        arms: "Golden armbands and bracelets. Often holds a crook and flail.",
        accessories: "Numerous golden amulets, rings, and other royal regalia.",
        overallStyle: "Extremely opulent and regal, reflecting his status as pharaoh."
    },

    // Add more characters as needed...
  };

  if (clothingDescriptions[character]) {
    return clothingDescriptions[character];
  } else {
    return "Character clothing description not found.";
  }
}

// Example usage:
const keferClothing = describeEgyxosClothing("Kefer");
console.log(keferClothing);

const exatonClothing = describeEgyxosClothing("Exaton");
console.log(exatonClothing);

const apisClothing = describeEgyxosClothing("Apis");
console.log(apisClothing);

const tutClothing = describeEgyxosClothing("Tutankhamun");
console.log(tutClothing);

const unknownClothing = describeEgyxosClothing("UnknownCharacter");
console.log(unknownClothing);
