function _valToString(val)
{
    if (val === undefined || val === null)
        return '[' + typeof(val) + ']';
    return val.toString() + '[' + typeof(val) + ']';
}

function _assert(cond, text)
{
    assert_true(!!cond, text);
}

function _assertSame(a, b, text_a, text_b)
{
    var msg = text_a + ' === ' + text_b + ' (got ' + _valToString(a) +
              ', expected ' + _valToString(b) + ')';
    assert_equals(a, b, msg);
}

function _assertDifferent(a, b, text_a, text_b)
{
    var msg = text_a + ' !== ' + text_b + ' (got ' + _valToString(a) +
              ', expected not ' + _valToString(b) + ')';
    assert_not_equals(a, b, msg);
}


function _getPixel(canvas, x,y)
{
    var ctx = canvas.getContext('2d');
    var imgdata = ctx.getImageData(x, y, 1, 1);
    return [ imgdata.data[0], imgdata.data[1], imgdata.data[2], imgdata.data[3] ];
}

function _assertPixel(canvas, x,y, r,g,b,a, pos, colour)
{
    var c = _getPixel(canvas, x,y);
    assert_equals(c[0], r, 'Red channel of the pixel at (' + x + ', ' + y + ')');
    assert_equals(c[1], g, 'Green channel of the pixel at (' + x + ', ' + y + ')');
    assert_equals(c[2], b, 'Blue channel of the pixel at (' + x + ', ' + y + ')');
    assert_equals(c[3], a, 'Alpha channel of the pixel at (' + x + ', ' + y + ')');
}

function _assertPixelApprox(canvas, x,y, r,g,b,a, pos, colour, tolerance)
{
    var c = _getPixel(canvas, x,y);
    assert_approx_equals(c[0], r, tolerance, 'Red channel of the pixel at (' + x + ', ' + y + ')');
    assert_approx_equals(c[1], g, tolerance, 'Green channel of the pixel at (' + x + ', ' + y + ')');
    assert_approx_equals(c[2], b, tolerance, 'Blue channel of the pixel at (' + x + ', ' + y + ')');
    assert_approx_equals(c[3], a, tolerance, 'Alpha channel of the pixel at (' + x + ', ' + y + ')');
}

let _deferred = false;

function deferTest() {
  _deferred = true;
}

function _addTest(testFn)
{
    on_event(window, "load", function()
    {
        t.step(function() {
            var canvas = document.getElementById('c');
            var ctx = canvas.getContext('2d');
            t.step(testFn, window, canvas, ctx);
        });

        if (!_deferred) {
            t.done();
        }
    });
}

function _assertGreen(ctx, canvasWidth, canvasHeight)
{
    var testColor = function(d, idx, expected) {
        assert_equals(d[idx], expected, "d[" + idx + "]", String(expected));
    };
    var imagedata = ctx.getImageData(0, 0, canvasWidth, canvasHeight);
    var w = imagedata.width, h = imagedata.height, d = imagedata.data;
    for (var i = 0; i < h; ++i) {
        for (var j = 0; j < w; ++j) {
            testColor(d, 4 * (w * i + j) + 0, 0);
            testColor(d, 4 * (w * i + j) + 1, 255);
            testColor(d, 4 * (w * i + j) + 2, 0);
            testColor(d, 4 * (w * i + j) + 3, 255);
        }
    }
}

function addCrossOriginYellowImage()
{
    var img = new Image();
    img.id = "yellow.png";
    img.className = "resource";
    img.src = get_host_info().HTTP_REMOTE_ORIGIN + "/images/yellow.png";
    document.body.appendChild(img);
}

function addCrossOriginRedirectYellowImage()
{
    var img = new Image();
    img.id = "yellow.png";
    img.className = "resource";
    img.src = get_host_info().HTTP_ORIGIN + "/common/redirect.py?location=" +
        get_host_info().HTTP_REMOTE_ORIGIN + "/images/yellow.png";
    document.body.appendChild(img);
}
