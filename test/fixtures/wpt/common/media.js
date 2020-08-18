/**
 * Returns the URL of a supported video source based on the user agent
 * @param {string} base - media URL without file extension
 * @returns {string}
 */
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

/**
 * Returns the URL of a supported audio source based on the user agent
 * @param {string} base - media URL without file extension
 * @returns {string}
 */
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

/**
 * Returns the MIME type for a media URL based on the file extension.
 * @param {string} url
 * @returns {string}
 */
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
