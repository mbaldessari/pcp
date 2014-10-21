#!/usr/bin/python
# pcp_stats - pcp(1) report graphing utility
# Copyright (C) 2014  Michele Baldessari
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.

from __future__ import print_function
from __future__ import division
import bisect
from datetime import datetime
from hashlib import sha1
from itertools import repeat
import multiprocessing
import os
import re
import resource
#import shutil
import sys
import tempfile

from reportlab.platypus.paragraph import Paragraph
from reportlab.platypus import PageBreak, Image, Spacer, Table
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import inch

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.colors as colors
import matplotlib.cm as cm
from matplotlib.patches import Rectangle

from pcp2pdf_style import PcpDocTemplate
from pcp2pdf_archive import PcpArchive, PcpHelp
import cpmapi as c_api

# When showing a rectangle when the interval is > than the average frequency we
# first multiply by FREQUNCY ERROR in order to avoid spurious rectangles
FREQUENCY_ERROR = 1.1

# To debug memory leaks
USE_MELIAE = False
if USE_MELIAE:
    from meliae import scanner, loader
    import objgraph

# DPI used to create matplotimages. It's fairly high
# in order for the graph to be crisp and clear
# The higher, the more RAM is needed when creating
# the pdf
DPI = 200
# FIXME: these two need to be moved to pcp2pdf_style
# Inch graph size (width, height)
GRAPH_SIZE = (10.5, 6.5)
# Axis (title, fontsize, dateformat, locator in min)
X_AXIS = ('Time', 12, '%m-%d %H:%M', 20)

# Threshold above which the legend is placed on the bottom
# of the page
LEGEND_THRESHOLD = 50


def ellipsize(text, limit=100):
    '''Truncates a string in a nice-formatted way'''
    ret = text[:limit].rsplit(' ', 1)[0]
    if len(ret) > limit - 3:
        ret = ret + '...'
    return ret

def progress_callback(graph_added):
    if graph_added:
        sys.stdout.write('.')
    else:
        sys.stdout.write('-')
    sys.stdout.flush()

def split_chunks(list_to_split, chunksize): 
    """Split the list l in chunks of at most n in size""" 
    return [list_to_split[i:i+chunksize] for i in range(0, len(list_to_split), chunksize)] 

def graph_wrapper((pcparch_obj, data)):
    """This is a wrapper due to pool.map() single argument limit"""
    (label, fname, metrics, text, indomres) = data
    ret = pcparch_obj.create_graph(fname, label, metrics, indomres)
    progress_callback(ret)
    return ((label, fname, metrics, text, indomres), ret)

def print_mem_usage(data):
    usage = resource.getrusage(resource.RUSAGE_SELF)
    print("Graphing: {0} usertime={1} systime={2} mem={3} MB"
        .format(data, usage[0], usage[1],
        (usage[2] / 1024.0)))

def match_res(patterns, string, flags=0):
    if type(string) != str:
        string = str(string)
    for pattern in patterns:
        ret = re.match(pattern, string, flags)
        # FIXME remove this
        #print("Pattern: {0} - String: {1} --> {2}".format(pattern, string, ret))
        if ret is not None:
            return ret
    return None

# Python 2.6 does not have total_seconds() in datetime objects
def total_seconds(timedelta):
    return timedelta.days * 3600 * 24 + timedelta.seconds + timedelta.microseconds / 100000.0


class PcpStats(object):
    story = []

    def __init__(self, args, opts):
        self.args = args
        self.pcphelp = PcpHelp()
        self.pcparchive = PcpArchive(args, opts)
        self.raw = opts.raw
        self.labels = opts.labels
        self.threaded = opts.threaded
        self.configparser = opts.configparser
        # Using /var/tmp as /tmp is ram-mounted these days
        self.tempdir = tempfile.mkdtemp(prefix='pcpstats', dir='/var/tmp')
        # This will contain all the metrics found in the archive file
        self.all_data = {}
        # Verify which set of metrics are to be used
        self.metrics = []
        if not opts.include and not opts.exclude:
            self.metrics = sorted(self.pcparchive.get_metrics())
        elif opts.include and not opts.exclude: # Only include filter specified
            metrics = sorted(self.pcparchive.get_metrics())
            for i in opts.include:
                try:
                    matched = filter(lambda x: re.match(i, x), metrics)
                except:
                    print("Failed to parse: {0}".format(i))
                    sys.exit(-1)
                self.metrics.extend(matched)
        elif not opts.include and opts.exclude: # Only exclude filter specified
            metrics = sorted(self.pcparchive.get_metrics())
            matched = []
            for i in opts.exclude:
                try:
                    matched.extend(filter(lambda x: re.match(i, x), metrics))
                except:
                    print("Failed to parse: {0}".format(i))
                    sys.exit(-1)

            self.metrics = sorted(list(set(metrics) - set(matched)))
        else:
            all_metrics = sorted(self.pcparchive.get_metrics())
            matched = []
            for i in opts.exclude:
                try:
                    matched.extend(filter(lambda x: re.match(i, x), all_metrics))
                except:
                    print("Failed to parse: {0}".format(i))
                    sys.exit(-1)

            delta_metrics = sorted(list(set(all_metrics) - set(matched)))
            metrics = sorted(self.pcparchive.get_metrics())
            for i in opts.include:
                try:
                    matched = filter(lambda x: re.match(i, x), metrics)
                except:
                    print("Failed to parse: {0}".format(i))
                    sys.exit(-1)
                delta_metrics.extend(matched)
            self.metrics = delta_metrics

        self.custom_graphs = []
        # Verify if there are any custom graphs. The can be defined like this:
        # "traffic:network.interface.out.bytes:eth0,network.interface.in.bytes:virbr[0-9]*,network.tcp..*:.*"
        for graph in opts.custom_graphs:
            try:
                x = graph.find(':')
                label = graph[0:x]
                line = graph[x + 1:]
                elements = line.split(',')
            except:
                print("Failed to parse label: {0}".format(graph))
                sys.exit(-1)

            if label in self.metrics:
                print("Cannot use label {0}. It is an existing metric".format(label))
                sys.exit(-1)

            all_metrics = sorted(self.pcparchive.get_metrics())
            indomres = {}
            metrics = []
            for element in elements:
                try:
                    (metric_str, indom_str) = element.split(':')
                except:
                    print("Failed to parse: {0}".format(element))
                    sys.exit(-1)
                try:
                    tmp_metrics = filter(lambda x: re.match(metric_str, x), all_metrics)
                    metrics.extend(tmp_metrics)
                except:
                    print("Failed to parse: {0}".format(metric_str))
                    sys.exit(-1)
                for metric in tmp_metrics:
                    if metric in indomres:
                        indomres[metric].append(indom_str)
                    else:
                        indomres[metric] = [indom_str]

            # Try to compile the indom_res to make sure they are valid
            errors = []
            for pattern in indomres:
                try:
                    re.compile(pattern)
                except:
                    errors.append(pattern)
                    pass

            if len(errors) > 0:
                print("Invalid regular expressions: {0}".format(
                    " ".join(errors)))
                sys.exit(-1)

            # We expanded all the metrics here. We cannot do the same for indoms as those
            # are not yet available. We just pass the regexes and do it at custom
            # graph creation time
            self.custom_graphs.append(("custom.%s" % label, metrics, indomres))

        try: # Not all matplotlib versions have this key
            matplotlib.rcParams['figure.max_open_warning'] = 100
        except KeyError:
            pass

    def _graph_filename(self, metrics, extension='.png'):
        '''Creates a unique constant file name given a list of metrics'''
        # We're on python 2.6o .jpg even though graph quality is affected,
        # because the underlying imaging lib bails out on a few graphs from
        # time to time
        pyver = sys.version_info
        if pyver[0] == 2 and pyver[1] <= 6:
            extension = '.jpg'
        if isinstance(metrics, list):
            temp = ''
            for i in metrics:
                temp += i
        else:
            temp = "_".join(metrics)
        fname = os.path.join(self.tempdir, temp + extension)
        return fname

    def _do_heading(self, text, sty):
        if isinstance(text, list):
            text = "_".join(text)
        # create bookmarkname
        bn = sha1(text + sty.name).hexdigest()
        # modify paragraph text to include an anchor point with name bn
        h = Paragraph(text + '<a name="%s"/>' % bn, sty)
        # store the bookmark name on the flowable so afterFlowable can see this
        h._bookmarkName = bn
        self.story.append(h)

    def rate_convert(self, timestamps, values):
        '''Given a list of timestamps and a list of values it will return the
        following:
        [[t1, t2, ..., tN], [(v1-v0)/(t1-t0), (v2-v1)/(t2-t1), ..., (vN-vN-1)/(tN -tN-1)]
        '''
        if len(timestamps) != len(values):
            raise Exception('Len of timestamps must be equal to len of values')
        new_timestamps = []
        new_values = []
        for t in range(1, len(timestamps)):
            delta = timestamps[t] - timestamps[t-1]
            new_timestamps.append(delta)

        for v in range(1, len(values)):
            seconds = total_seconds(new_timestamps[v-1])
            try:
                delta = (values[v] - values[v-1]) / seconds
            except ZeroDivisionError:
                # If we have a zero interval but the values difference is zero
                # return 0 anyway
                if values[v] - values[v-1] == 0:
                    delta = 0
                    pass
                else:
                    # if the delta between the values is not zero try to use
                    # the previous calculated delta
                    if v > 1:
                        delta = new_values[v - 2]
                    else: # In all other cases just set the delta to 0
                        delta = 0
                    pass

            new_values.append(delta)

        # Add previous datetime to the time delta
        for t in range(len(new_timestamps)):
            ts = new_timestamps[t]
            new_timestamps[t] = ts + timestamps[t]

        return (new_timestamps, new_values)

    def find_max(self, timestamp, metrics):
        '''Given data as returned by pcparchive.get_values a timestamp and a set of metrics,
        find the maximum y value. If the given timestamp does not exist in the data we
        do a linear interpolation'''
        max_value = -sys.maxint
        for metric in metrics:
            for indom in self.all_data[metric]:
                timestamps = self.all_data[metric][indom][0]
                y_values = self.all_data[metric][indom][1]
                try:
                    x = timestamps.index(timestamp)
                    y = y_values[timestamp]
                except ValueError:
                    time_key_right = bisect.bisect_right(timestamps, timestamp)
                    # If the custom label's timestamp falls outside the
                    # data we have we skip this
                    if time_key_right >= len(timestamps):
                        continue
                    time_key_left = time_key_right - 1
                    x1 = mdates.date2num(timestamps[time_key_left])
                    x2 = mdates.date2num(timestamps[time_key_right])
                    y1 = y_values[time_key_left]
                    y2 = y_values[time_key_right]
                    if x1 == x2 or y1 == y2: # No need to do any interpolation
                        y = y1
                    else:
                        m = (y2 - y1) / (x2 - x1)
                        x = mdates.date2num(timestamp) - x1
                        y = m * x + y1

                if y > max_value:
                    max_value = y
        return max_value

    def get_frequency(self, data):
        # First we calculate the observed frequency (in seconds) of the observed measurements
        total = 0.0
        counter = 0
        for metric in data:
            for indom in data[metric]:
                timestamps = data[metric][indom][0]
                last = None
                for time in timestamps:
                    if not last:
                        last = time
                        continue
                    delta = total_seconds(time - last)
                    total += delta
                    counter += 1
                    last = time

        frequency = total / counter
        return frequency

    def find_data_gaps(self, data):
        '''Returns a dictionary with tuples containing the start and end of the large intervals
        as tuples. The value of the dictionary is a list of tuples where this interval has been
        observed (metric, indom).
        Returns: {(gap1start, gap1end): [(metric, indom), (metric2, indom2), ...],
                  {gap2start, gap2end): [(metric, indom), (metric2, indom2), ...]}'''

        frequency = self.get_frequency(data)
        ret = {}
        for metric in data:
            for indom in data[metric]:
                timestamps = data[metric][indom][0]
                last = None
                for time in timestamps:
                    if not last:
                        last = time
                        continue
                    delta = total_seconds(time - last)
                    if delta > frequency * FREQUENCY_ERROR:
                        key = (last, time)
                        if key not in ret:
                            ret[key] = [(metric, indom)]
                        else:
                            ret[key].append((metric, indom))
                    last = time
        return ret


    def parse(self):
        '''Parses the archive and stores all the metrics in self.all_data. Returns a dictionary
        containing the metrics which have been rate converted'''

        (all_data, self.skipped_graphs) = self.pcparchive.get_values(progress=progress_callback)
        print(' total of {0} graphs'.format(len(all_data)), end='')
        if len(self.skipped_graphs) > 0:
            print(' - skipped {0} graphs'.format(len(self.skipped_graphs)), end='')

        rate_converted = {}
        # Prune all the sets of values where all values are zero as it makes
        # no sense to show those
        for metric in all_data:
            rate_converted[metric] = False
            tmp = {}
            # FIXME: Once python 2.6 dep is dropped we can use the following
            #tmp = {key: value for key, value in all_data[metric].items()
            #       if not all([ v == 0 for v in value[1]])}
            data = all_data[metric].items()
            for key, value in data:
                if not all([ v == 0 for v in value[1]]):
                    tmp[key] = value
            
            if len(tmp) > 0:
                self.all_data[metric] = tmp

        print(' - total of non-fully zeroed graphs {0}'.format(len(self.all_data)), end='')


        if self.raw: # User explicitely asked to not rate convert any metrics
            return rate_converted

        # Rate convert all the PM_SEM_COUNTER metrics
        for metric in self.all_data:
            (mtype, msem, munits, dtype, desc_units, desc_type) = self.pcparchive.get_metric_info(metric)
            if msem != c_api.PM_SEM_COUNTER:
                continue

            for indom in self.all_data[metric]:
                data = self.all_data[metric][indom]
                (ts, val) = self.rate_convert(data[0], data[1])
                self.all_data[metric][indom] = [ts, val]
                if rate_converted[metric] == False:
                    rate_converted[metric] = {}
                rate_converted[metric][indom] = True

        return rate_converted

    def get_category(self, label, metrics):
        '''Return the category given one or a list of metric strings'''
        if isinstance(metrics, str):
            if label.startswith('Custom'):
                return 'Custom'
            return metrics.split('.')[0]
        elif isinstance(metrics, list):
            if label.startswith('Custom'):
                return 'Custom'
            category = None
            for metric in metrics:
                prefix = metric.split('.')[0]
                if category == None and prefix != category:
                    category = prefix
                elif category != None and prefix != category:
                    raise Exception('Multiple categories in %s' % metrics)
            return category.title()
        else:
            raise Exception('Cannot find category for %s' % metrics)

    def is_string_metric(self, metric):
        '''Given a metric returns True if values' types are strings'''
        data = self.all_data[metric]
        isstring = False
        for indom in data:
            values = data[indom][1]
            if all([isinstance(v, str) for v in values]):
                isstring = True
                break
        return isstring

    def create_graph(self, fname, title, metrics, indomres):
        '''Take a filename, a title, a list of metrics and an indom_regex to
        create an image of the graph'''
        # reportlab has a 72 dpi by default
        fig = plt.figure(figsize=(GRAPH_SIZE[0], GRAPH_SIZE[1]))
        axes = fig.add_subplot(111)
        # Set X Axis metadata
        axes.set_xlabel(X_AXIS[0])
        axes.set_title('{0} time series'.format(title, fontsize=X_AXIS[1]))
        axes.xaxis.set_major_formatter(mdates.DateFormatter(X_AXIS[2]))
        axes.xaxis.set_minor_locator(mdates.MinuteLocator(interval=X_AXIS[3]))
        fig.autofmt_xdate()

        # Set Y Axis metadata
        axes.set_ylabel(title)
        y_formatter = matplotlib.ticker.ScalarFormatter(useOffset=False)
        axes.yaxis.set_major_formatter(y_formatter)
        axes.yaxis.get_major_formatter().set_scientific(False)
        found = False
        indoms = 0
        counter = 0

        # First we calculate the maximum number of colors needed
        max_values_len = 0
        for metric in metrics:
            values = self.all_data[metric]
            count = 0
            for indom in values:
                if indomres is not None and metric in indomres:
                    if match_res(indomres[metric], indom) is None:
                        continue
                count += 1
            if count > max_values_len:
                max_values_len = count

        # We need at most number of max(indoms) * metrics colors
        vmax_color = max_values_len * len(metrics)
        color_norm = colors.Normalize(vmin=0, vmax=vmax_color)
        scalar_map = cm.ScalarMappable(norm=color_norm,
                                       cmap=plt.get_cmap('Set1'))

        # Then we walk the metrics and plot
        for metric in metrics:
            values = self.all_data[metric]
            for indom in sorted(values):
                # If the indomres is not None we use the indom only if the re string
                # matches
                if indomres is not None and metric in indomres:
                    if match_res(indomres[metric], indom) is None:
                        continue
                (timestamps, dataset) = values[indom]
                # Currently if there is only one (timestamp,value) like with filesys.blocksize
                # we just do not graph the thing
                if len(timestamps) <= 1:
                    continue
                if len(metrics) > 1:
                    if indom == 0:
                        lbl = metric
                    else:
                        lbl = "%s %s" % (metric, indom)
                else:
                    if indom == 0:
                        lbl = title
                    else:
                        lbl = indom

                found = True
                try:
                    axes.plot(timestamps, dataset, 'o:', label=lbl,
                              color=scalar_map.to_rgba(counter))
                except:
                    import traceback
                    print("Metric: {0}".format(metric))
                    print(traceback.format_exc())
                    sys.exit(-1)

                indoms += 1
                counter += 1

        if not found:
            return False

        # Show any data collection gaps in the graph
        gaps = self.find_data_gaps(self.all_data).keys()
        if len(gaps) > 0:
            for i in gaps:
                (g1, g2) = i
                x1 = mdates.date2num(g1)
                x2 = mdates.date2num(g2)
                (ymin, ymax) = plt.ylim()
                axes.add_patch(Rectangle((x1, ymin), x2 - x1, ymax-ymin, facecolor="lightgrey"))

        axes.grid(True)

        # Draw self.labels if non empty
        if len(self.labels) > 0:
            for label in self.labels:
                max_value = self.find_max(self.labels[label], metrics)
                # should we not find a max_value at all (due to empty timestamps)
                if max_value == -sys.maxint:
                    max_value = 0
                axes.annotate(label, xy=(mdates.date2num(self.labels[label]),
                    max_value), xycoords='data', xytext=(30, 30), textcoords='offset points',
                    arrowprops=dict(arrowstyle="->", connectionstyle="arc3,rad=.2"))

        # Add legend only when there is more than one instance
        lgd = False
        if indoms > 1:
            fontproperties = matplotlib.font_manager.FontProperties(size='xx-small')
            if indoms > LEGEND_THRESHOLD:
                # Draw legend on the bottom only when instances are more than LEGEND_THRESHOLD
                lgd = axes.legend(loc=9, ncol=int(indoms**0.6), bbox_to_anchor=(0.5, -0.29),
                                  shadow=True, prop=fontproperties)
            else:
                # Draw legend on the right when instances are more than LEGEND_THRESHOLD
                lgd = axes.legend(loc=1, ncol=int(indoms**0.5), shadow=True, prop=fontproperties)

        if lgd:
            plt.savefig(fname, bbox_extra_artists=(lgd,), bbox_inches='tight', dpi=DPI)
        else:
            plt.savefig(fname, bbox_inches='tight', dpi=DPI)
        plt.cla()
        plt.clf()
        plt.close('all')
        if USE_MELIAE:
            objgraph.show_growth()
            tmp = tempfile.mkstemp(prefix='pcp-test')[1]
            scanner.dump_all_objects(tmp)
            leakreporter = loader.load(tmp)
            summary = leakreporter.summarize()
            print(summary)
        return True

    def get_all_graphs(self):
        '''Returns all the graphs that need to be plotted
        Prepare the full list of graphs that will be drawn
        Start with any custom graphs if they exist and
        proceed with the remaining ones. Split the metrics
        that have string values into a separate array
        all_graphs = [(label, fname, (m0, m1, .., mN), text), ...]'''

        all_graphs = []
        string_metrics = []
        indom_res = []

        for graph in self.custom_graphs:
            (label, metrics, indom_res) = graph
            fname = self._graph_filename(label)
            text = None
            custom_metrics = []
            for metric in metrics: # verify that the custom graph's metrics actually exist
                if metric in self.all_data:
                    custom_metrics.append(metric)

            if len(custom_metrics) > 0:
                if isinstance(metrics, str) and metrics in self.pcphelp.help_text:
                    text = '<strong>%s</strong>: %s' % (metrics, self.pcphelp.help_text[metrics])
                all_graphs.append((label, fname, custom_metrics, text, indom_res))

        for metric in sorted(self.all_data):
            # Make sure that we plot only the metrics that the
            # user has specified
            if not metric in self.metrics:
                continue

            if self.is_string_metric(metric):
                string_metrics.append(metric)
            else:
                fname = self._graph_filename([metric])
                units_str = self.pcparchive.get_metric_info(metric)[4]
                type_str = self.pcparchive.get_metric_info(metric)[5]
                if isinstance(metric, str) and metric in self.pcphelp.help_text:
                    help_text = self.pcphelp.help_text[metric]
                else:
                    help_text = '...'

                text = '<strong>%s</strong>: %s (%s - %s)' % (metric, help_text, units_str, type_str)
                if self.rate_converted[metric] != False:
                    text = text + ' - <em>%s</em>' % 'rate converted'
                all_graphs.append((metric, fname, [metric], text, None))

        return (all_graphs, string_metrics)

    def output(self, output_file='output.pdf'):
        # FIXME: Split this function in smaller pieces. This is unreadable
        sys.stdout.write('Parsing archive: ')
        sys.stdout.flush()
        self.rate_converted = self.parse()
        print()
        doc = PcpDocTemplate(output_file, self.configparser, pagesize=landscape(A4))
        hostname = self.pcparchive.get_hostname()
        self._do_heading('%s' % hostname, doc.fonts["centered_index"])
        self.story.append(Spacer(1, 0.10 * inch))
        self.story.append(Image('pcplogo.png'))
        #self.story.append(Paragraph('%s' % hostname, doc.fonts["centered"]))
        self.story.append(Spacer(1, 0.10 * inch))
        self.story.append(Paragraph('PCP Archives: %s' % (" ".join(self.args)),
                          doc.fonts["mono"]))

        self.story.append(Paragraph('Start: %s - End: %s' %
            (datetime.fromtimestamp(self.pcparchive.start),
            datetime.fromtimestamp(self.pcparchive.end)), doc.fonts["mono"]))
        self.story.append(Paragraph('Interval: %s seconds' %
            (self.pcparchive.interval), doc.fonts["mono"]))
        self.story.append(Spacer(1, 0.10 * inch))

        #self._do_heading('Table of contents', doc.fonts["centered_index"])
        #self.story.append(doc.toc)
        self.story.append(PageBreak())

        (self.all_graphs, string_metrics) = self.get_all_graphs()
        done_metrics = []
        # This list contains the metrics that contained data
        print('Creating graphs: ', end='')
        if self.threaded:
            pool = multiprocessing.Pool(None)
            l = zip(repeat(self), self.all_graphs)
            metrics_rets = pool.map(graph_wrapper, l)
            (metrics, rets) = zip(*metrics_rets)
            done_metrics = [metric for (metric, ret) in metrics_rets if ret]
        else:
            for graph in self.all_graphs:
                (label, fname, metrics, text, indomres) = graph
                if self.create_graph(fname, label, metrics, indomres):
                    progress_callback(True)
                    done_metrics.append(graph)
                else:
                    # Graphs had all zero values
                    progress_callback(False)

        print()
        # Build the string metrics table. It only prints
        # a value if it changed over time
        data = [('Metric', 'Timestamp', 'Value')]
        for metric in string_metrics:
            last_value = None
            for indom in self.all_data[metric]:
                timestamps = self.all_data[metric][indom][0]
                values = self.all_data[metric][indom][1]
                for (ts, v) in zip(timestamps, values):
                    if last_value != v:
                        text = ellipsize(v)
                        data.append((metric, '%s' % ts, text))
                        last_value = v

        if len(data) > 1:
            self._do_heading('String Metrics', doc.fonts["heading1"])
            self.story.append(Spacer(1, 0.2 * inch))
            width = doc.pagesize[0]
            table = Table(data, colWidths=(0.15 * width, 0.2 * width, 0.6 * width))
            table.setStyle(doc.tablestyle)
            self.story.append(table)
            self.story.append(PageBreak())

        # At this point all images are created let's build the pdf
        print("Building pdf: ", end='')
        # Add the graphs to the pdf
        last_category = ''
        for graph in done_metrics:
            (label, fname, metrics, text, indom_res) = graph
            category = self.get_category(label, metrics)
            if last_category != category:
                self._do_heading(category, doc.fonts["heading1"])
                last_category = category

            self._do_heading(label, doc.fonts["heading2_invisible"])
            self.story.append(Image(fname, width=GRAPH_SIZE[0]*inch,
                              height=GRAPH_SIZE[1]*inch))
            if text:
                self.story.append(Paragraph(text, doc.fonts["normal"]))
            self.story.append(PageBreak())
            sys.stdout.write('.')
            sys.stdout.flush()

        doc.multiBuild(self.story)
        print()
        print("Done building: {0}".format(output_file))
        #FIXME
        #shutil.rmtree(self.tempdir)
        print("Done removing: {0}".format(self.tempdir))

    def print_info(self):
        # FIXME: print start and end of the archive
        # Print the metrics
        d = {}
        for metric in self.metrics:
            (prefix, metric) = metric.split('.', 1)
            if prefix in d:
                d[prefix].append(metric)
            else:
                d[prefix] = []

        try:
            rows, columns = os.popen('stty size', 'r').read().split()
        except:
            columns = 80
        columns = int(columns) - 10

        import textwrap
        for prefix in sorted(d):
            line = ", ".join(sorted(d[prefix]))
            indent = ' ' * (len(prefix) + 2)
            text = textwrap.fill(line, width=columns, initial_indent='',
                                 subsequent_indent=indent)
            print('{0}: {1}'.format(prefix, text))