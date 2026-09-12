#pragma once

//Non-zero if a link stub in LinkStubsESP32.cpp was ever entered. Every stub that can be
//reached only when a premise of the cut is wrong bumps this; the ones a station legitimately
//calls do not, so a non-zero value is an alarm and not a tally. Always 0 in a build that did
//not take the cut. See LinkStubsESP32.cpp and
//misc\docs\plans\lightbulb-esp32-pcf8574-size-audit-plan.md.
uint32_t LeifGetLinkStubHits();
