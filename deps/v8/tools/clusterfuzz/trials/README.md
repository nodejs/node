It is possible to add trials from the source code side by adding the below line
to `clusterfuzz_trials_config.json`:

```
{
  "app_args": "FLAG_NAME",
  "app_name": "APP_NAME",
  "probability": PROBABILITY,
  "contradicts": ["FLAG1", "FLAG2", ...]
}
```

- `app_args`: the name of the flag we want to add.
- `app_name`: the name of the app we are adding the flag for, this must be `d8` on `V8`.
- `probability`: the probability of this flag to be selected.
- `contradicts` (optional): the flags that contradict the flag referred to in `app_args`.
This will stop flags that contradict each other from being added in the same
trial.
