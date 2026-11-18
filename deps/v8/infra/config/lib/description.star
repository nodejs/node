def to_html(name, bucket, description):
    html = "%s<br/><br/>" % description["purpose"]
    request = description.get("request", None)
    if request:
        html += "Request: <a href='%s'>%s</a><br/>" % (request, request)
    html += _equivalent_builder_line(description, bucket, name)
    html += _triggering_line(description, bucket)
    html += _compiles_with_line(description, bucket)
    html += _configuration_line(description, name)
    return html

def _equivalent_builder_line(description, bucket, name):
    # Compile only builders should set this equivalent property, but in case of pairs
    # we only need the base name to solve it's value.
    equivalent = description.get("equivalent", None)
    eqv_bucket = description.get("eqv_bucket", None)
    html = ""
    if bucket.startswith("ci"):
        html += "CQ"
        cq_base = description.get("cq_base", None)
        if cq_base:
            if name.endswith("builder"):
                prefix, suffix = cq_base.rsplit("_", 1)
                equivalent = prefix + "_compile_" + suffix
                eqv_bucket = "try"
            else:
                equivalent = cq_base
                eqv_bucket = "try"
    elif bucket.startswith("try"):
        html += "CI"
        ci_base = description.get("ci_base", None)
        if ci_base:
            if name.endswith(("_compile_rel", "_compile_dbg")):
                equivalent = ci_base + " - builder"
            else:
                equivalent = ci_base
        eqv_bucket = "ci"
    html += " equivalent: <a href='https://ci.chromium.org/p/v8/builders/%s/%s'>%s</a><br/>" % (
        eqv_bucket,
        equivalent,
        equivalent,
    )
    return html

def _triggering_line(description, bucket):
    html = ""
    triggers = description.get("triggers", None)
    if triggers:
        html += "Triggers: <a href='https://ci.chromium.org/p/v8/builders/ci/%s'>%s</a><br/>" % (
            triggers,
            triggers,
        )
    triggered_by = description.get("triggered by", None)
    if triggered_by:
        html += "Triggered by: <a href='https://ci.chromium.org/p/v8/builders/ci/%s'>%s</a><br/>" % (
            triggered_by,
            triggered_by,
        )
    return html

def _compiles_with_line(description, bucket):
    compiles_with = description.get("compiles_with", None)
    if compiles_with:
        return "Compiles with: <a href='https://ci.chromium.org/p/v8/builders/try/%s'>%s</a><br/>" % (
            compiles_with,
            compiles_with,
        )
    return ""

def _configuration_line(description, name):
    # Set kind to builder/tester on builders that do not follow current naming
    # convention.
    default_kind = "builder" if (
        name.endswith(" builder") or name.endswith("_ng")
    ) else "tester"
    builder_kind = description.get("kind", default_kind)
    if builder_kind == "builder":
        return "<a href='https://source.chromium.org/chromium/chromium/src/+/main:v8/infra/mb/mb_config.pyl?q=%22%27" + \
               name + "%27%22'>Compiler arguments</a><br/>"
    elif builder_kind == "tester":
        return "<a href='https://source.chromium.org/chromium/chromium/src/+/main:v8/infra/testing/builders.pyl?q=%22%27" + \
               name + "%27%22'>Tester configuration</a><br/>"
    return ""  # any other future kind of builder
