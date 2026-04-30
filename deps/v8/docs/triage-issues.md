---
title: 'Triaging issues'
description: 'This document explains how to deal with issues in V8’s bug tracker.'
---
This document explains how to deal with issues in [V8’s bug tracker](/bugs).

## How to get an issue triaged

- *V8 tracker*: Set the state to `Untriaged`
- *Chromium tracker*: Set the state to `Untriaged` and add the component `Blink>JavaScript`

## How to assign V8 issues in the Chromium tracker

Please move issues to the V8 specialty sheriffs queue of one of the
following categories:

- Memory: `component:blink>javascript status=Untriaged label:Performance-Memory`
    - Will show up in [this](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3Ablink%3Ejavascript+status%3DUntriaged+label%3APerformance-Memory+&colspec=ID+Pri+M+Stars+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles) query
- Stability: `status=available,untriaged component:Blink>JavaScript label:Stability -label:Clusterfuzz`
    - Will show up in [this](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=status%3Davailable%2Cuntriaged+component%3ABlink%3EJavaScript+label%3AStability+-label%3AClusterfuzz&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids) query
    - No CC needed, will be triaged by a sheriff automatically
- Performance: `status=untriaged component:Blink>JavaScript label:Performance`
    - Will show up in [this](https://bugs.chromium.org/p/chromium/issues/list?colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Cr%20Status%20Owner%20Summary%20OS%20Modified&x=m&y=releaseblock&cells=tiles&q=component%3Ablink%3Ejavascript%20status%3DUntriaged%20label%3APerformance&can=2) query
    - No CC needed, will be triaged by a sheriff automatically
- Clusterfuzz: Set the bug to the following state:
    - `label:ClusterFuzz component:Blink>JavaScript status:Untriaged`
    - Will show up in [this](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=label%3AClusterFuzz+component%3ABlink%3EJavaScript+status%3AUntriaged&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids) query.
    - No CC needed, will be triaged by a sheriff automatically
- Security: Please see [V8's security policy](https://chromium.googlesource.com/v8/v8/+/HEAD/SECURITY.md) for more information.

If you need the attention of a sheriff, please consult the rotation information.

Use the component `Blink>JavaScript` on all issues.

**Please note that this only applies to issues tracked in the Chromium issue tracker.**
