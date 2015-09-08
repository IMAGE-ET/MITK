'''
Created on Aug 26, 2015

@author: wirkert
'''

import os
import numpy as np
import copy

import luigi
import SimpleITK as sitk

from msi.io.nrrdreader import NrrdReader
from msi.io.nrrdwriter import NrrdWriter
import msi.msimanipulations as msimani
from msi.normalize import standard_normalizer
import scriptpaths as sp


def calc_max_flatfield(flatfield1, flatfield2):
    """ calculate maximum of two flatfields """
    flatfield = copy.copy(flatfield1)
    maximum_of_image_array = np.maximum(flatfield1.get_image(),
                                        flatfield2.get_image())
    flatfield.set_image(maximum_of_image_array)
    return flatfield


def smooth(msi):
    """ helper function to gaussian smooth msi channel by channel. """
    img = sitk.GetImageFromArray(msi.get_image(), isVector=True)
    smoothFilter = sitk.SmoothingRecursiveGaussianImageFilter()
    smoothFilter.SetSigma(4)
    img_smoothed = smoothFilter.Execute(img)
    img_array = sitk.GetArrayFromImage(img_smoothed)
    msi.set_image(img_array)


def resort_wavelengths(msi):
    """ as a standard, no resorting takes place. rebind this method
    if wavelengths need sorting """
    return msi


def touch_and_save_msi(msi, outfile):
    """ saves msi as a nrrd to outfile.
    if the directory / file does not exist it will be created """
    # touch file so path definately exists
    _outFile = outfile.open('w')
    _outFile.close()
    # use nrrd writer to write file
    _out = outfile
    writer = NrrdWriter(msi)
    writer.write(_out.path)


class MultiSpectralImageFile(luigi.Task):
    """
    the unaltered file c.f. hard disk
    """
    imageName = luigi.Parameter()

    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.FAP_IMAGE_FOLDER,
                                self.imageName))


class CorrectImagingSetupTask(luigi.Task):
    """unfortunately filter were ordered weirdly. this task is to
    do all the fiddeling to get it right again"""
    imageName = luigi.Parameter()

    def requires(self):
        return MultiSpectralImageFile(imageName=self.imageName), \
            PreprocessFlatfields(), \
            PreprocessDark()


    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.RESULTS_FOLDER,
                                self.imageName +
                                "_image_setup_corrected.nrrd"))

    def run(self):
        """sort wavelengths and normalize by respective integration times"""
        reader = NrrdReader()
        msi = reader.read(self.input()[0].path)
        if "long" in self.input()[0].path:
            msi.add_property({'integration times':
                np.array([150., 250., 117., 160., 150., 175., 82., 70.])})
        else:
            msi.add_property(
                    {'integration times':
                     np.array([30., 50., 47., 32., 30., 35., 35., 60.])})
        flatfield = reader.read(self.input()[1].path)
        flatfield.add_property({'integration times':
                     np.array([30., 50., 47., 32., 30., 35., 35., 60.])})
        dark = reader.read(self.input()[2].path)
        msimani.image_correction(msi, flatfield, dark)
        resort_wavelengths(msi)
        touch_and_save_msi(msi, self.output())


class PreprocessFlatfields(luigi.Task):

    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.RESULTS_FOLDER,
                                  "flatfield.nrrd"))

    def requires(self):
        return luigi.Task.requires(self)

    def run(self):
        reader = NrrdReader()
        flatfield_folder = os.path.join(sp.ROOT_FOLDER, sp.FLAT_FOLDER)
        flatfield_nrrds = os.listdir(flatfield_folder)
        #  map to full file path
        flatfield_nrrds = \
            [os.path.join(flatfield_folder, f) for f in flatfield_nrrds]
        flatfields = map(reader.read, flatfield_nrrds)
        max_flatfield = reduce(calc_max_flatfield, flatfields)

        # apply sitk gaussian smoothing to flatfield result
        smooth(max_flatfield)
        touch_and_save_msi(max_flatfield, self.output())


class PreprocessDark(luigi.Task):

    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.RESULTS_FOLDER,
                                  "dark.nrrd"))

    def requires(self):
        return luigi.Task.requires(self)

    def run(self):
        reader = NrrdReader()
        dark_folder = os.path.join(sp.ROOT_FOLDER, sp.DARK_FOLDER)
        dark_nrrds = os.listdir(dark_folder)
        #  map to full file path
        dark_nrrds = [os.path.join(dark_folder, d) for d in dark_nrrds]
        dark = reader.read(dark_nrrds[0])  # just take the first dark image
        # alternatively multiple dark images could be averaged to one.
        # apply sitk gaussian smoothing to dark result
        smooth(dark)
        touch_and_save_msi(dark, self.output())


class PreprocessMSI(luigi.Task):
    imageName = luigi.Parameter()

    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.RESULTS_FOLDER,
                                  self.imageName + "_preprocessed.nrrd"))

    def requires(self):
        return CorrectImagingSetupTask(imageName=self.imageName)


    def run(self):
        reader = NrrdReader()
        image = reader.read(self.input().path)
        standard_normalizer.normalize(image)
        touch_and_save_msi(image, self.output())


class SegmentMSI(luigi.Task):
    """
    in this class we segment the msi. We filter out saturated
    (dark and bright) pixels.
    The remaining pixels are taken for domain adaptation
    """
    imageName = luigi.Parameter()

    def requires(self):
        return MultiSpectralImageFile(self.imageName), \
            PreprocessMSI(self.imageName)

    def output(self):
        return luigi.LocalTarget(os.path.join(sp.ROOT_FOLDER,
                                              sp.RESULTS_FOLDER,
                                  self.imageName + "_segmentation.nrrd"))

    def run(self):
        reader = NrrdReader()
        msi_image = reader.read(self.input()[0].path).get_image()
        preprocessed_msi_image = reader.read(self.input()[1].path).get_image()
        max_low_wavelengths = \
            np.max(preprocessed_msi_image[:, :, [0, 1, 3, 4, 5]], axis=-1)
        min_high_wavelengths = \
            np.min(preprocessed_msi_image[:, :, [2, 6, 7]], axis=-1)

        # do "blood test"
        segmentation = max_low_wavelengths < min_high_wavelengths
        # filter dark spots
        segmentation = np.logical_and(segmentation,
                                     (np.max(msi_image, axis=-1) > 400.))
        # filter bright spots
        segmentation = np.logical_and(segmentation,
                                     (np.max(msi_image, axis=-1) < 4000.))

        img = sitk.GetImageFromArray(np.uint8(segmentation))
        outFile = self.output().open('w')
        outFile.close()
        sitk.WriteImage(img, self.output().path)

