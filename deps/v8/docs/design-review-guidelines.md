---
title: 'Design review guidelines'
description: 'This document explains the V8 project’s design review guidelines.'
---
Please make sure to follow the following guidelines when applicable.

There are multiple drivers for the formalization of V8’s design reviews:

1. make it clear to Individual Contributors (ICs) who the decision makers are and highlight what the path forward in the case that projects are not proceeding due to technical disagreement
1. create a forum to have straight-forward design discussions
1. ensure V8 Technical Leads (TL) are aware of all significant changes and have the opportunity to give their input on the Tech Lead (TL) layer
1. increase the involvement of all V8 contributors over the globe

## Summary

![V8’s Design Review Guidelines at a glance](/_img/docs/design-review-guidelines/design-review-guidelines.svg)

Important:

1. assume good intentions
1. be kind and civilized
1. be pragmatic

The proposed solution is based on the following assumption/pillars:

1. The proposed workflow puts the individual contributor (IC) in charge. They are the one who facilitate the process.
1. Their guiding TLs are tasked to help them navigate the territory and find the right LGTM providers.
1. If a feature is uncontroversial, nearly no overhead should be created.
1. If there is a lot of controversy, the feature can be 'escalated' to the V8 Eng Review Owners meeting where further steps are decided.

## Roles

### Individual Contributor (IC)

LGTM: N/A
This person is the creator of the feature and the creator of the design documentation.

### The Tech Lead (TL) of the IC

LGTM: Must have
This person is the TL of a given project or component. Likely this is the person that is an owner of the main component your feature is going to touch. If it is not clear who the TL is, please ask the V8 Eng Review Owners via v8-eng-review-owners@googlegroups.com. TLs are responsible to add more people to the list of required LGTMs if appropriate.

### LGTM provider

LGTM: Must have
This is a person that is required to give LGTM. It might be an IC, or a TL(M).

### “Random” reviewer of the document (RRotD) { #rrotd }

LGTM: Not required
This is somebody who is simply reviewing and comment on the proposal. Their input should be considered, although their LGTM is not required.

### V8 Eng Review Owners

LGTM: Not required
Stuck proposals can be escalated to the V8 Eng Review Owners via <v8-eng-review-owners@googlegroups.com>. Potential use cases of such an escalation:

- an LGTM provider is non-responsive
- no consensus on the design can be reached

The V8 Eng Review Owners can overrule non-LGTMs or LGTMs.

## Detailed workflow

![V8’s Design Review Guidelines at a glance](/_img/docs/design-review-guidelines/design-review-guidelines.svg)

1. Start: IC decides to work on a feature/gets a feature assigned to them
1. IC sends out their early design doc/explainer/one pager to a few RRotDs
    1. Prototypes are considered part of the "design doc"
1. IC adds people to the list of LGTM providers that the IC thinks should give their LGTM. The TL is a must have on the list of LGTM providers.
1. IC incorporates feedback.
1. TL adds more people to the list of LGTM providers.
1. IC sends out the early design doc/explainer/one pager to <v8-dev+design@googlegroups.com>.
1. IC collects the LGTMs. TL helps them.
    1. LGTM provider reviews document, add comments and gives either an LGTM or not LGTM at the beginning of the document. If they add a not LGTM, they are obligated to list the reason(s).
    1. Optional: LGTM providers can remove themselves from the list of LGTM providers and/or suggest other LGTM providers
    1. IC and TL work to resolve the unresolved issues.
    1. If all LGTM are gathered send an email to v8-dev@googlegroups.com (e.g. by pinging the original thread) and announce implementation.
1. Optional: If IC and TL are blocked and/or want to have a broader discussion they can escalate the issue to the V8 Eng Review Owners.
    1. IC sends a mail to v8-eng-review-owners@googlegroups.com
        1. TL in CC
        1. Link to design doc in the mail
    1. Every V8 Eng Review Owners member is obligated to review the doc and optionally add themselves to the list of LGTM providers.
    1. Next steps to unblock the feature are decided.
    1. If the blocker is not resolved afterwards or new, unresolvable blockers are discovered, goto 8.
1. Optional: If "not LGTMs" are added after the feature was approved already, they should be treated like normal, unresolved issues.
    1. IC and TL work to resolve the unresolved issues.
1. End: IC proceeds with the feature.

And always remember:

1. assume good intentions
1. be kind and civilized
1. be pragmatic

## FAQ

### How to decide if the feature is worthy to have a design document?

Some pointers when a design doc is appropriate:

- Touches at least two components
- Needs reconciliation with non-V8 projects e.g. Debugger, Blink
- Take longer than 1 week of effort to implement
- Is a language feature
- Platform specific code will be touched
- User facing changes
- Has special security consideration or security impact is not obvious

When in doubt, ask the TL.

### How to decide who to add to the list of LGTM providers?

Some pointers when people should be added to the list of LGTM providers:

- OWNERs of the source files/directories you anticipate to touch
- Main component expert of the components you anticipate to touch
- Downstream consumers of your changes e.g. when you change an API

### Who is “my” TL?

Likely this is the person that is an owner of the main component your feature is going to touch. If it is not clear who the TL is, please ask the V8 Eng Review Owners via <v8-eng-review-owners@googlegroups.com>.

### Where can I find a template for design documents?

[Here](https://docs.google.com/document/d/1CWNKvxOYXGMHepW31hPwaFz9mOqffaXnuGqhMqcyFYo/template/preview).

### What if something big changes?

Make sure you still have the LGTMs e.g. by pinging the LGTM providers with a clear, reasonable deadline to veto.

### LGTM providers don’t comment on my doc, what should I do?

In this case you can follow this path of escalation:

- Ping them directly via mail, Hangouts or comment/assignment in the doc and specifically ask them explicitly to add an LGTM or non-LGTM.
- Get your TL involved and ask them for help.
- Escalate to <v8-eng-review-owners@googlegroups.com>.

### Somebody added me as an LGTM provider to a doc, what should I do?

V8 is aiming to make decisions more transparent and escalation more straight-forward. If you think the design is good enough and should be done add an “LGTM” to the table cell next to your name.

If you have blocking concerns or remarks, please add “Not LGTM, because \<reason>” into the table cell next to your name. Be prepared to get asked for another round of review.

### How does this work together with the Blink Intents process?

The V8 Design Review Guidelines complement [V8’s Blink Intent+Errata process](/docs/feature-launch-process). If you are launching a new WebAssembly or JavaScript language feature, please follow V8’s Blink Intent+Errata process and the V8 Design Review Guidelines. It likely makes sense to have all the LGTMs gathered at the point in time you would send an Intent to Implement.
