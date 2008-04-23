import wx
from provami.QueryChoice import QueryChoice

class LevelsChoice(QueryChoice):
    def __init__(self, parent, model):
        QueryChoice.__init__(self, parent, model, "level", "levels")
        self.hasData("levels")

    def readFilterFromRecord(self, rec):
        return self.model.getLevelFilter(rec)

    def readOptions(self):
        res = []
        res.append(("All levels", None))
        for lev in self.model.levels():
            res.append(("%d,%d,%d,%d" % lev, lev))
        return res

    def selected(self, event):
        if self.updating: return
        sel = self.GetSelection()
        lev = self.GetClientData(sel)
        if lev == None:
            self.model.setLevelFilter(None)
        else:
            self.model.setLevelFilter(lev)
