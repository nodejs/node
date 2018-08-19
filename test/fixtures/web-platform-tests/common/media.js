//
// Returns the URI of a supported video source based on the user agent
//
function getVideoURI(base)
{
    var extension = '.mp4';

    var videotag = document.createElement("video");

    if ( videotag.canPlayType  &&
         videotag.canPlayType('video/ogg; codecs="theora, vorbis"') )
    {
        extension = '.ogv';
    }

    return base + extension;
}

//
// Returns the URI of a supported audio source based on the user agent
//
function getAudioURI(base)
{
    var extension = '.mp3';

    var audiotag = document.createElement("audio");

    if ( audiotag.canPlayType &&
         audiotag.canPlayType('audio/ogg') )
    {
        extension = '.oga';
    }

    return base + extension;
}

function getMediaContentType(url) {
    var extension = new URL(url, location).pathname.split(".").pop();
    var map = {
        "mp4": "video/mp4",
        "ogv": "video/ogg",
        "mp3": "audio/mp3",
        "oga": "audio/ogg",
    };
    return map[extension];
}
