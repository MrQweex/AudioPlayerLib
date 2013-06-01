#include "AudioPlayerGnu.h"

///////////////////////////Private

gboolean AudioPlayerGnu::bus_callback(GstBus *bus, GstMessage *msg, gpointer callingGnu)
{
    switch(GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        if(((AudioPlayerGnu*)callingGnu)->finishListener)
          ((AudioPlayerGnu*)callingGnu)->finishListener->playingFinished();
        gst_element_set_state(((AudioPlayerGnu*)callingGnu)->pipeline, GST_STATE_READY);
        g_debug("end-of-stream");
        break;
    case GST_MESSAGE_ERROR:
        gchar *debug;
        GError *err;
        gst_message_parse_error(msg, &err, &debug);
        g_free(debug);
        g_warning("Error: %s", err->message);
        g_error_free(err);
        std::cerr << "ERROR" << std::endl;
        break;
    default:
        break;
    }
    return TRUE;
}

void AudioPlayerGnu::init(int *argc, char **argv[])
{
    gst_init(argc, argv);
}

void AudioPlayerGnu::setWindow(gpointer window_)
{
    window = window_;
}

void AudioPlayerGnu::reset()
{
    gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, (GstSeekFlags) seek_flags, GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

///////////////////////////Public

AudioPlayerGnu::~AudioPlayerGnu()
{
    //TODO???
    //delete pipeline;
    //delete videosink;
}

void AudioPlayerGnu::play()
{
    //stop();
    if(GST_IS_X_OVERLAY(videosink))
        gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(videosink), GPOINTER_TO_INT(window));
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void AudioPlayerGnu::stop() //TODO
{
    if(!pipeline)
        return;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    pipeline = NULL;
}

void AudioPlayerGnu::pause()
{
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
}

void AudioPlayerGnu::seek(double t)
{
    gint value = t;
    gst_element_seek(pipeline,          //pipeline element
                     1.0,               //rate
                     GST_FORMAT_TIME,   //GstFormat?
                     (GstSeekFlags) seek_flags,        //flags??
                     GST_SEEK_TYPE_SET, //start_type; if you want it to start
                     value * GST_MSECOND,//Time to seek to
                     GST_SEEK_TYPE_NONE,//
                     GST_CLOCK_TIME_NONE);
}

double AudioPlayerGnu::duration() const
{
    GstFormat format = GST_FORMAT_TIME;
    gint64 cur;
    gboolean result = gst_element_query_duration(pipeline, &format, &cur);
    if(!result || format != GST_FORMAT_TIME)
        return GST_CLOCK_TIME_NONE;
    return cur;
}

double AudioPlayerGnu::progress() const
{
    GstFormat format = GST_FORMAT_TIME;
    gint64 cur;
    gboolean result = gst_element_query_position(pipeline, &format, &cur);
    if(!result || format != GST_FORMAT_TIME)
        return GST_CLOCK_TIME_NONE;
    return cur;
}


bool AudioPlayerGnu::isPlaying() const
{
    GstState current, pending;
    gst_element_get_state(pipeline, &current, &pending,0);
    std::cout << "derp " << current << "==" << GST_STATE_PLAYING << " (" << pending << ")" << std::endl;
    return current==GST_STATE_PLAYING;
}

bool AudioPlayerGnu::isPaused() const
{
    GstState current, pending;
    gst_element_get_state(pipeline, &current, &pending,0);
    std::cout << "derp " << current << "==" << GST_STATE_PAUSED << " (" << pending << ")" << std::endl;
    return current==GST_STATE_PAUSED;
}

bool AudioPlayerGnu::isStopped() const
{
    GstState current, pending;
    gst_element_get_state(pipeline, &current, &pending,0);
    std::cout << "derp " << current << "==" << GST_STATE_NULL << " (" << pending << ")" << std::endl;
    return current==GST_STATE_NULL;
}

    /*GstElement:
     *  GST_STATE_VOID_PENDING = 0
     *  GST_STATE_NULL = 1
     *  GST_STATE_READY = 2
     *  GST_STATE_PAUSED = 3
     *  GST_STATE_PLAYING = 4
     */
AudioPlayerGnu* AudioPlayerGnu::file(const char *fn)
{
    AudioPlayerGnu* a = new AudioPlayerGnu();
    a->init(NULL, NULL);
    a->filename = fn;
    a->pipeline = gst_element_factory_make("playbin", "gst-player");
    a->videosink = gst_element_factory_make("xvimagesink", "videosink");

    g_object_set(G_OBJECT(a->pipeline), "video-sink", a->videosink, NULL);

    {
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(a->pipeline));
        gst_bus_add_watch(bus, bus_callback, gpointer(a));
        gst_object_unref(bus);
    }

    {
        gchar *uri;
        if(gst_uri_is_valid(a->filename))
        {
            uri = g_strdup(a->filename);
        }
        else if(g_path_is_absolute(a->filename))
        {
            uri = g_filename_to_uri(a->filename, NULL, NULL);
        }
        else
        {
            gchar *tmp = g_build_filename(g_get_current_dir(), a->filename, NULL);
            uri = g_filename_to_uri(tmp, NULL, NULL);
            g_free(tmp);
        }

        g_debug("%s", uri); //Printing
        g_object_set(G_OBJECT(a->pipeline), "uri", uri, NULL);
        g_free(uri);
    }
    g_object_set(G_OBJECT(a->videosink), "force-aspect-ratio", TRUE, NULL);


    /*GST_STATE_CHANGE_
     *              FAILURE = 0
     *              SUCCESS = 1
     *              ASYNC = 2
     *              NO_PREROLL = 3
     */
    gst_element_set_state(a->pipeline, GST_STATE_PAUSED); //Have to pause first to test if it is a valid file
    GstState current, pending;
    GstStateChangeReturn isValidFile = gst_element_get_state(a->pipeline, &current, &pending,0);

    if(!isValidFile)
    {
        delete a; return NULL;
    }

    return a;
}

void AudioPlayerGnu::setFinishListener(AudioPlayerCallback* cbo)
{
    finishListener = cbo;
}

void AudioPlayerGnu::setVolume(int percentage)  //percentage ranges 0 to 100
{
    volumeForMute = percentage;
    g_object_set(G_OBJECT(pipeline), "volume", percentage/10.0/2, NULL); //range from 1.0 to 10.0???
}

int AudioPlayerGnu::getVolume() const
{
    double v;
    g_object_get(G_OBJECT(pipeline),"volume", &v, NULL);
    int i=v+0;
    return i;
}

void AudioPlayerGnu::mute()
{
    if(isMuted)
        g_object_set(G_OBJECT(pipeline), "volume", 0, NULL);
    isMuted=true;
}

void AudioPlayerGnu::unmute()
{
    if(!isMuted)
        setVolume(volumeForMute);
    isMuted=false;
}

void AudioPlayerGnu::toggleMute()
{
    if(isMuted)
        unmute();
    else
        mute();
}
