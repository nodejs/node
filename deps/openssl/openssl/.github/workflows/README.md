## Rationale about our design for the GitHub Actions CI

The balance is between the time taken and the number of jobs.
We're allowed 180 concurrent jobs in total across the entire project.
Currently we're running about 60 on pull_request, a few more on push and
a pile per day.
So three simultaneous PRs should finish quickly enough.
Given that most jobs run quickly, this could scale up to 5 or 6 without
problem.

Moving more jobs into the `pull_request` category will limit the number
of parallel builds (from different PRs) we can handle.
We got into quite some strife over this with our older CI hosts
-- remember builds taking the best part of a day to run.
We really want to avoid that again.

I've been trying to limit total job time per job to around 20-30 minutes
(there are some longer ones I know of), with most jobs running in the
sub 5 minute range.
There are some longer lived CIs -- up to an hour and I try to delegate
these to push or daily rather than pull_request.

Still, there is no hard and fast rule about what runs when or where.
Make a suggestion about bettering the CIs -- Ideally I'd like the
`pull_request` jobs to be the ones catching most of the problems and the
push and daily being predictably boring successes.
Just make an effort to rationally justify the inclusions/changes.

Things like the sanitiser builds, we know catch problems often.
So even though they are slow they are worthwhile on `pull_request`.
A lot of the daily builds are unlikely to catch much since they are
checking options can be turned off and on, so they are fine not running
as much.
The demarkation between `pull_request` and `pull_request + push` is the
difficult choice.
I believe we should do all pull_request jobs as part of push too.
The question is how many more should there be.

I don't have a good answer but I think we're converging on a practical
number and we should get better as we gain experience.
